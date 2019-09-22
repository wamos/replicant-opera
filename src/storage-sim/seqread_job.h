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

class SeqReadJob {
private:
    HDFSDriver dataset;
    SimpleCluster cluster;
    ISimulator *simulator;
    uint64_t block_id; 

    double map_time = 88.315;
    double sort_time = 71.276;
    double sort_start_time = 25;
    
    void runSeqWrite(){
        // for seq write: we have a metadata update and a list of pending blocks
        // task dependency: metadata-update req/resp -> block-write 0 -> block-write 1 -> block-write 2
        // -> ack from 2 to 1 -> ack from 1 to 0 -> ack to client

    }

    bool SortFn (uint64_t i, uint64_t j) { return (i<j); }

    void BlockConstruct(){
        for (uint64_t file_id = 0; file_id < dataset.file_count; file_id++) {
            std::vector<uint64_t> block_list;
            for (uint64_t block_offset = 0; block_offset < dataset.GetBlockCount(); block_offset++) {
                block_id = block_id + 1;
                dataset.placeBlocks(file_id, block_id, 0);
                dataset.placeBlocks(file_id, block_id, 1);
                dataset.placeBlocks(file_id, block_id, 2);
                block_list.push_back(block_id);
            }
            dataset.updateBlockList(file_id, block_list);
        }
    }

    void BlockWrite(uint64_t file_id){
        std::vector<uint64_t> block_list;
        for (uint64_t block_offset = 0; block_offset < dataset.GetBlockCount(); block_offset++) {
            block_id = block_id + 1;
            dataset.placeBlocks(file_id, block_id, 0);
            dataset.placeBlocks(file_id, block_id, 1);
            dataset.placeBlocks(file_id, block_id, 2);
            block_list.push_back(block_id);
        }
        dataset.updateBlockList(file_id, block_list);
    }

    Flow* AddFlowForTask(RWTask* task, uint64_t host_id, double flow_start){
        auto& client   = cluster.hosts[0];
        auto& namenode = cluster.hosts[1];
        if(task->dst_id == namenode.hostid){ // meta req
            task->flows.push_back(simulator->AddFlow(host_id, namenode.hostid, METADATA_SIZE, flow_start, task)->flow_id);
        }
        else if(host_id == namenode.hostid){ // metadata resp
            task->flows.push_back(simulator->AddFlow(host_id, client.hostid, METADATA_SIZE, flow_start, task)->flow_id);
        }
        else{
            auto f = simulator->AddFlow(host_id, task->dst_id, dataset.block_size, flow_start, task);
            task->flows.push_back(f->flow_id);
            //std::cout << "flow added\n";   
        }

    }
    
    void runSeqRead() {

        // This host_to_file_blocks is like god-view knowledge, we should 
        //map<uint64_t, queue<tuple<uint64_t, uint64_t>>> host_to_file_blocks; 
        uint64_t task_dispatched = 0;
        uint64_t task_completed = 0;

        // each block neeeds 2 steps: metadata-read req/resp -> block-read req/resp
        uint64_t task_total_count = dataset.file_count * 2 ;
        task_total_count += dataset.file_count * (uint64_t)dataset.GetBlockCount() * 2;
        //task_total_count += (uint64_t)dataset.GetBlockCount() * 2;

        //prepare blocks (not files) for seq-read, each block needs an unique id
        BlockConstruct();
        auto& client   = cluster.hosts[0];
        auto& namenode = cluster.hosts[1];


        // per file based mete-data retrival 
        for (uint64_t file_id = 0; file_id < dataset.file_count; file_id++) {
            // client to namenode tasks
            RWTask *client_task = new RWTask(client.hostid, namenode.hostid, 0, file_id, "C2NN");
            client.pending_tasks.push(client_task);
            //std::cout<<+client_task->task_id<<"\n";

            // namenode to clinet tasks
            RWTask *namenode_task = new RWTask(namenode.hostid, client.hostid, 0, file_id, "NN2C");
            namenode_task->setDependency(client_task);
            namenode.pending_tasks.push(namenode_task);
            //std::cout<<+namenode_task->task_id<<"\n";
        }

        //per-block based data read
        for (uint64_t file_id = 0; file_id < dataset.file_count; file_id++) { 
            //for (uint64_t file_id = 0; file_id < 1; file_id++) {
            std::vector<uint64_t> blocklist = dataset.getBlockList(file_id);
            for(auto block_id: blocklist){
                std::vector<uint64_t> locList = dataset.getBlockLocations(block_id);
                uint64_t dst_id = locList[0];
                //std::sort(locList.begin(), locList.end());
                //uint64_t dst_id = locList[0]; // the closet one to client

                //TODO: Does read_req need to depend on corresponding client_task?
                //nope we got queue to maintain the order?
                RWTask *read_req = new RWTask(client.hostid, dst_id, 0, file_id, block_id, "C2DN");
                client.pending_tasks.push(read_req);
                //std::cout<<"resp dst_id:"<<+read_req->dst_id<<"\n";
                //std::cout<<"req taskid:"<<+read_req->task_id<<"\n";

                RWTask *read_resp = new RWTask(dst_id, client.hostid, 0, file_id, block_id, "DN2C");
                read_resp->setDependency(read_req);
                auto& dst_node = cluster.hosts[dst_id];
                dst_node.pending_tasks.push(read_resp);
                //std::cout<<"resp dst_id:"<<+read_resp->dst_id<<"\n";
                //std::cout<<"resp taskid:"<<+read_resp->task_id<<"\n";
            }
            
        }

        /*for (int index=0; index< cluster.hosts.size();index++) {
            SimpleNode node = cluster.hosts[index];
            int qsize = node.pending_queue.size();
            std::cout << "hostid:"<< node.hostid << ",size:" << qsize <<"\n";
            for(RWTask* task : node.pending_tasks){
                std::cout<<"dst id:"<<+task->dst_id<<",";
            }
             std::cout << "\n";
        }*/
                    
        std::cout << "Pre while \n";
        //put tasks in queues to work, a task needs to check whether its upstream task is finished
        while (task_completed < task_total_count) {
        //while (task_completed < 1) {
            //launch meta tasks
            std::cout << "Pre for\n";
            for (int index=0; index< cluster.hosts.size();index++) {
                auto& node = cluster.hosts[index];
                //while (node.pending_tasks.size() < (size_t)cluster.core_count && !node.pending_queue.empty()) {
                while (node.pending_tasks.size() > 0 && node.running_tasks.size() < (size_t)cluster.core_count) {
                    auto current_time =simulator->GetCurrentTime();
                    auto flow_start = current_time + 0.5; // I don't know, just assume there is some delay time
                    //uint64_t task_id = node.pending_queue.front();
                    RWTask* task = node.pending_tasks.front();
                    if(task->dst_id > 10){
                        throw out_of_range("Illegal dst, we only have "+to_string(cluster.GetTotalNodeCount())+" nodes");
                    }
                    
                    if(task->dep_task == nullptr){
                        std::cout << "Task "<< task->task_id<<" launched\n"; 
                        auto f = AddFlowForTask(task, node.hostid, flow_start);
                        //auto f = simulator->AddFlow(node.hostid, task->dst_id, dataset.block_size, flow_start, task);
                        //task->flows.push_back(f->flow_id);
                        node.pending_tasks.pop();
                        node.running_tasks.insert(make_pair(task->task_id, task));
                        task_dispatched++;
                        //std::cout << "flow added\n";
                    }
                    else{ //task->dep_task != nullptr
                        if(task->dep_task->isCompleted == false){
                            std::cout << "Task " << task->task_id <<" is waiting for its upstream tasks "<< task->dep_task->task_id <<"\n";
                            // we got a head-of-line block, since we use queues for tasks 
                            break;
                        }
                        else{
                            std::cout << "Task "<< task->task_id<<" launched with dep task ready\n"; 
                            auto f = AddFlowForTask(task, node.hostid, flow_start);
                            //auto f = simulator->AddFlow(node.hostid, task->dst_id, dataset.block_size, flow_start, task);
                            //task->flows.push_back(f->flow_id);
                            node.pending_tasks.pop();
                            node.running_tasks.insert(make_pair(task->task_id, task));
                            task_dispatched++;
                            //std::cout << "flow added\n";   
                        }
                    }
                    
                    /*
                        //task needs to know data size
                        if(task->dst_id == namenode.hostid){ // meta req
                            task->flows.push_back(simulator->AddFlow(host_id, namenode.hostid, METADATA_SIZE, flow_start, task)->flow_id);
                        }
                        else if(host_id == namenode.hostid){ // metadata resp
                            task->flows.push_back(simulator->AddFlow(host_id, client.hostid, METADATA_SIZE, flow_start, task)->flow_id);
                        }
                        else{
                        auto f = simulator->AddFlow(host_id, task->dst_id, dataset.block_size, flow_start, task);
                        task->flows.push_back(f->flow_id);
                        std::cout << "flow added\n";   
                        }
                    */

                    
                    //node.pending_queue.pop();
                    //node.pending_tasks.erase(task->task_id);
                    //node.running_tasks.insert(make_pair(task->task_id, task));
                    //task_dispatched++;
                } //while loop                    
            }
            //simulator->printFlows();
            

            auto completed_flows = simulator->RunToNextCompletion();
            /*if(completed_flows.size()==0){
                break;
            }*/
            //task_completed++;     
            for (auto *flow: completed_flows) {
                auto host_id = flow->task->host_id;
                auto &node = cluster.hosts[host_id];
                if (node.running_tasks.find(flow->task->task_id) == node.running_tasks.end())
                    throw runtime_error("Task " + to_string(flow->task->task_id) + " not found.");
                auto &flows = node.running_tasks[flow->task->task_id]->flows;
                //flows.erase(remove(flows.begin(), flows.end(), flow->flow_id), flows.end());
                std::remove(flows.begin(), flows.end(), flow->flow_id);
                if (flows.empty()) {
                    flow->task->end_time = simulator->GetCurrentTime();
                    node.finished_tasks.insert(make_pair(flow->task->task_id, flow->task));
                    node.running_tasks.erase(flow->task->task_id);
                    flow->task->isCompleted = true;
                    std::cout << flow->task->task_id << " is completed\n";
                    //std::cout << "Completed:"<< task_completed <<"\n";
                    task_completed++;
                }
            }
        }

#if MY_DEBUG
        fprintf(stderr, "Map tasks include %lu intra-rack flows and %lu inter-rack flows.\n", intra_rack_flow_count, inter_rack_flow_count);
#endif
        printf("Seq Read Done\n");
    }

    void PrintTaskCompletionTimes() {
        map<string, map<double, size_t>> fcts;

        for (auto &[host_id, node]: cluster.hosts) {
            for (auto &[task_id, task]: node.finished_tasks) {
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
    SeqReadJob(HDFSDriver dataset, ISimulator *simulator)
        : dataset(dataset), cluster(dataset.cluster), simulator(simulator), block_id(0){}

    void Run() {
        runSeqRead();
        simulator->PrintFlowCompletionTimes();
        //PrintTaskCompletionTimes();
    }
};

#endif //SEQREAD_H
 