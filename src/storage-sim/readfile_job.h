#ifndef FLOW_SIM_SEQREAD_H
#define FLOW_SIM_SEQREAD_H

#include <cstdint>
#include <cinttypes>
#include <iostream>
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

class ReadFileJob {
private:
    HDFSDriver dataset;
    SimpleCluster cluster;
    ISimulator *simulator;
    uint64_t block_id; 
    uint64_t file_id;

    void BlockConstruct_onefile_oneblock(){
        std::vector<uint64_t> block_list;
        dataset.placeBlocks(file_id, block_id, 0);
        dataset.placeBlocks(file_id, block_id, 1);
        dataset.placeBlocks(file_id, block_id, 2);
        block_list.push_back(block_id);
        dataset.updateBlockList(file_id, block_list);
        file_id = file_id + 1; //prepare for the next one
    }

    uint64_t FileWrite(){
        std::vector<uint64_t> block_list;
        for (uint64_t block_offset = 0; block_offset < dataset.GetBlockCount(); block_offset++) {
            dataset.placeBlocks(file_id, block_id, 0);
            dataset.placeBlocks(file_id, block_id, 1);
            dataset.placeBlocks(file_id, block_id, 2);
            block_list.push_back(block_id);
            block_id = block_id + 1;
        }
        dataset.updateBlockList(file_id, block_list);
        uint64_t current_file_id = file_id;
        file_id = file_id + 1; //prepare for the next one
        return current_file_id;
    }

    Flow* AddFlowForTask(RWTask* task, uint64_t host_id, double flow_start){
        auto f = simulator->AddFlow(host_id, task->dst_id, FILE_SIZE, flow_start, task);
        task->flows.push_back(f->flow_id);
    }
    
    void ReadOneFile() {
        uint64_t task_dispatched = 0;
        uint64_t task_completed = 0;

        uint64_t task_total_count = 0;
        auto& client   = cluster.hosts[0];
        auto& server = cluster.hosts[1];

        RWTask *read_req = new RWTask(client.hostid, server.hostid, 0, file_id, block_id, "OneFlow");
        client.pending_tasks.push(read_req);
        task_total_count++;
        std::cout<<"req dst_id:"<<+read_req->dst_id<<"\n";
        std::cout<<"req taskid:"<<+read_req->task_id<<"\n";

        while (task_completed < task_total_count) {
            while (client.pending_tasks.size() > 0 && client.running_tasks.size() < 2) {
                auto current_time =simulator->GetCurrentTime();
                auto flow_start = current_time;
                RWTask* task = client.pending_tasks.front();
                if(task->dst_id > 10){
                    throw out_of_range("Illegal dst, we only have "+to_string(cluster.GetTotalNodeCount())+" nodes");
                }
                std::cout << "Task "<< task->task_id<<" launched\n"; 
                auto f = simulator->AddFlow(client.hostid, task->dst_id, FILE_SIZE, flow_start, task);
                task->flows.push_back(f->flow_id);
                client.pending_tasks.pop();
                client.running_tasks.insert(make_pair(task->task_id, task));
                task_dispatched++;
            }
            //simulator->printFlows();       

            auto completed_flows = simulator->RunToNextCompletion();

            for (auto *flow: completed_flows) {
                auto host_id = flow->task->host_id;
                auto &node = cluster.hosts[host_id];
                if (node.running_tasks.find(flow->task->task_id) == node.running_tasks.end())
                    throw runtime_error("Task " + to_string(flow->task->task_id) + " not found.");
                auto &flows = node.running_tasks[flow->task->task_id]->flows;
                flows.erase(remove(flows.begin(), flows.end(), flow->flow_id), flows.end());
                if (flows.empty()) {
                    flow->task->end_time = simulator->GetCurrentTime();
                    node.finished_tasks.insert(make_pair(flow->task->task_id, flow->task));
                    node.running_tasks.erase(flow->task->task_id);
                    flow->task->isCompleted = true;
                    std::cout << "task " << flow->task->task_id << " is completed\n";
                    task_completed++;
                }
            }
            std::cout << "Task dispatched:" << task_dispatched << ", Task compeleted:"<< task_completed << "\n";
        }
        std::cout << "Read Done\n";
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
    ReadFileJob(HDFSDriver dataset, ISimulator *simulator)
        : dataset(dataset), cluster(dataset.cluster), simulator(simulator), block_id(0), file_id(0){}

    void Run() {
        ReadOneFile();
        simulator->PrintFlowCompletionTimes();
        //PrintTaskCompletionTimes();
    }
};

#endif //SEQREAD_H
 