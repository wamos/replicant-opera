#ifndef FLOW_SIM_HADOOP_SORT_H
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

using namespace std;

class HadoopSort {
private:
    HadoopDataset dataset;
    HadoopCluster cluster;
    ISimulator *simulator;

    double map_time = 88.315;
    double sort_time = 71.276;
    double sort_start_time = 250;

    void RunShuffle() {
        uint64_t bucket_count = (uint64_t)cluster.reduce_core_count * cluster.GetTotalNodeCount();
        uint64_t bucket_size = dataset.block_size / bucket_count;
        printf("Shuffle: %" PRIu64 " files, %" PRIu64 " blocks per file, %" PRIu64 " nodes.\n",
                dataset.file_count, dataset.GetBlockCount(), cluster.GetTotalNodeCount());
        printf("Shuffle: %f MB/block, %llu buckets, %f MB/bucket.\n",
                (double)dataset.block_size / MB(1),
                bucket_count,
                (double)bucket_size / MB(1));
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

#if MY_DEBUG
        uint64_t intra_rack_flow_count = 0;
        uint64_t inter_rack_flow_count = 0;
#endif

        while (task_completed < task_total_count) {
            for (auto &[host_id, node]: cluster.hosts) {
                auto &src = host_id;
                while (node.map_tasks.size() < (size_t)cluster.map_core_count && !host_to_file_blocks[src].empty()) {
                    // Run map task, which takes some computation time and then send flow
                    auto current_time = simulator->GetCurrentTime();
                    auto &[file_id, block_id] = host_to_file_blocks[src].front();
                    MapTask *task = new MapTask(src, current_time, file_id, block_id);
                    auto flow_start = current_time + this->map_time;
                    for (uint64_t dst = 0; dst < cluster.GetTotalNodeCount(); dst++) {
                        if (src == dst)
                            continue;

                        for (int core = 0; core < cluster.reduce_core_count; core++) {
#if MY_DEBUG
                            if (cluster.GetRackId(src) == cluster.GetRackId(dst))
                                intra_rack_flow_count++;
                            else
                                inter_rack_flow_count++;
#endif
                            task->flows.push_back(simulator->AddFlow(src, dst, bucket_size, flow_start, task)->flow_id);
                        }
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

    void RunReduceOutput() {
        printf("Reduce: %" PRIu64 " files, %" PRIu64 " blocks per file.\n",
                dataset.file_count, dataset.GetBlockCount());
        map<uint64_t, vector<uint64_t>> host_to_files;
        for (uint64_t file_id = 0; file_id < dataset.file_count; file_id++) {
            uint64_t replica0 = dataset.GetNodeForBlock(file_id);
            host_to_files[replica0].push_back(file_id);
        }

        uint64_t task_dispatched = 0;
        for (auto &[host_id, node]: cluster.hosts) {
            const auto &file_ids = host_to_files.at(host_id);
            ReduceTask *task1 = new ReduceTask(host_id, sort_start_time, file_ids, "1");
            // TODO: this only supports 1 reducers per node.
            for (uint64_t file_id: file_ids) {
                for (uint64_t block_id = 0; block_id < dataset.GetBlockCount(); block_id++) {
                    uint64_t replica0 = host_id;
                    uint64_t replica1 = dataset.GetNodeForBlock(file_id, block_id, 1);
//                    uint64_t replica2 = dataset.GetNodeForBlock(file_id, block_id, 2);

                    double flow_start_time = sort_start_time + sort_time;
                    uint64_t flow_id;
                    flow_id = simulator->AddFlow(replica0, replica1, dataset.block_size, flow_start_time, task1)->flow_id;
                    task1->flows.push_back(flow_id);

/*
                    ReduceTask *task2 = new ReduceTask(replica1, sort_start_time, file_ids, "2");
                    flow_id = simulator->AddFlow(replica1, replica2, dataset.block_size, flow_start_time, task2)->flow_id;
                    task2->flows.push_back(flow_id);
                    cluster.hosts[replica1].reduce_tasks.insert(make_pair(task2->task_id, task2));
                    task_dispatched++;
*/
                }
            }

            node.reduce_tasks.insert(make_pair(task1->task_id, task1));
            task_dispatched++;
        }

        uint64_t task_completed = 0;
        while (task_completed < task_dispatched) {
            auto completed_flows = simulator->RunToNextCompletion();
            for (auto *flow: completed_flows) {
                auto host_id = flow->task->host_id;
                auto &node = cluster.hosts[host_id];
                if (node.reduce_tasks.find(flow->task->task_id) == node.reduce_tasks.end()) {
                    throw runtime_error("Reduce task " + to_string(flow->task->task_id) + " not found.");
                }
                auto &flows = node.reduce_tasks[flow->task->task_id]->flows;
                flows.erase(remove(flows.begin(), flows.end(), flow->flow_id), flows.end());
                if (flows.empty()) {
                    flow->task->end_time = simulator->GetCurrentTime();
                    node.completed_tasks.insert(make_pair(flow->task->task_id, flow->task));
                    node.reduce_tasks.erase(flow->task->task_id);
                    task_completed++;
                }
            }

            fprintf(stderr, "Task completed: %" PRIu64 "\n", task_completed);
        }

        printf("Reduce done\n");
    }

    void PrintTaskCompletionTimes() {
        map<string, map<double, size_t>> fcts;

        for (auto &[host_id, node]: cluster.hosts) {
            for (auto &[task_id, task]: node.completed_tasks) {
                double fct = task->end_time - task->start_time;
                fcts[task->type][fct]++;
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
    HadoopSort(HadoopDataset dataset, ISimulator *simulator)
        : dataset(dataset), cluster(dataset.cluster), simulator(simulator) {}

    void Run() {
        RunShuffle();
        simulator->PrintFlowCompletionTimes();

//        RunReduceOutput();
//        simulator->PrintFlowCompletionTimes();

        PrintTaskCompletionTimes();
    }
};

#endif  // FLOW_SIM_HADOOP_SORT_H
