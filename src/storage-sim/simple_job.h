#ifndef FLOW_SIM_SIMPLEJOB_H
#define FLOW_SIM_HADOOP_SORT_H

#include <cstdint>
#include <cinttypes>
#include <cmath>
#include <map>
#include <vector>
#include <tuple>
#include <string>
#include <queue>
#include <algorithm>
#include <functional>

#include "flowsim_sim_interface.h"
#include "flowsim_config_macro.h"
#include "hdfs_driver.h"
#include "simple_cluster.h"
#include "task.h"

using namespace std;

class SimpleJob {
private:
    HDFSDriver dataset;
    SimpleCluster cluster;
    ISimulator *simulator;

    double map_time = 88.315;
    double sort_time = 71.276;
    double sort_start_time = 250;

    void RunShuffle() {
        // Note: simplification: each block is assigned to a fixed node.
        map<uint64_t, queue<tuple<uint64_t, uint64_t>>> host_to_file_blocks;
        uint64_t task_dispatched = 0;
        uint64_t task_completed = 0;
        uint64_t task_total_count = dataset.file_count * (uint64_t)dataset.GetBlockCount();
        for (uint64_t file_id = 0; file_id < dataset.file_count; file_id++) {
            uint64_t host_id = dataset.GetNodeForBlock(file_id);
            auto &queue = host_to_file_blocks[host_id];
            for (uint64_t block_id = 0; block_id < dataset.GetBlockCount(); block_id++) {
                queue.push(make_tuple(file_id, block_id));
            }
        }

        while (task_completed < task_total_count) {
            // All-to-all communication pattern, assigning a block to a flow

            for (auto &[host_id, node]: cluster.hosts) {
                auto &src = host_id;
                while (node.map_tasks.size() < (size_t)cluster.core_count/2 && !host_to_file_blocks[src].empty()) {
                    // Run map task, which takes some computation time and then send flow
                    auto current_time = simulator->GetCurrentTime();
                    auto &[file_id, block_id] = host_to_file_blocks[src].front();
                    MapTask *task = new MapTask(src, current_time, file_id, block_id);
                    auto flow_start = current_time + this->map_time;
                    for (uint64_t dst = 0; dst < cluster.GetTotalNodeCount(); dst++) {
                        if (src == dst)
                            continue;
                        task->flows.push_back(simulator->AddFlow(src, dst, dataset.block_size, flow_start, task)->flow_id);    
                    }

                    node.map_tasks.insert(make_pair(task->task_id, task));
                    host_to_file_blocks[src].pop();
                    task_dispatched++;
                }
            }

            auto completed_flows = simulator->RunToNextCompletion();
            for (auto *flow: completed_flows) {
                auto host_id = flow->task->host_id;
                auto &node = cluster.hosts[host_id];
                if (node.map_tasks.find(flow->task->task_id) == node.map_tasks.end())
                    throw runtime_error("Map task " + to_string(flow->task->task_id) + " not found.");
                auto &flows = node.map_tasks[flow->task->task_id]->flows;
                flows.erase(remove(flows.begin(), flows.end(), flow->flow_id), flows.end());
                if (flows.empty()) {
                    flow->task->end_time = simulator->GetCurrentTime();
                    node.completed_tasks.insert(make_pair(flow->task->task_id, flow->task));
                    node.map_tasks.erase(flow->task->task_id);
                    task_completed++;
                }
            }

            fprintf(stderr, "Task dispatched: %" PRIu64 ", task completed: %" PRIu64 "\n", task_dispatched, task_completed);
        }

#if MY_DEBUG
        fprintf(stderr, "Map tasks include %lu intra-rack flows and %lu inter-rack flows.\n", intra_rack_flow_count, inter_rack_flow_count);
#endif
        printf("Shuffle done\n");
    }

    void PrintTaskCompletionTimes() {
        map<string, map<double, size_t>> fcts;

        for (auto &[host_id, node]: cluster.hosts) {
            for (auto &[task_id, task]: node.completed_tasks) {
                double fct = task->end_time - task->start_time;
                fcts[task->tag][fct]++;
            }
        }

        printf("======== Map/Reduce TCT ========\n");
        printf("Tag,FCT,Count\n");
        for (const auto &[tag, m]: fcts)
            for (const auto &[fct, count]: m) {
                printf("%s,%f,%zu\n", tag.c_str(), fct, count);
            }
        printf("======== End ========\n");
    }

public:
    SimpleJob(HDFSDriver dataset, ISimulator *simulator)
        : dataset(dataset), cluster(dataset.cluster), simulator(simulator) {}

    void Run() {
        RunShuffle();
        simulator->PrintFlowCompletionTimes();
        PrintTaskCompletionTimes();
    }
};

#endif  //FLOW_SIM_HADOOP_SORT_H
