#ifndef FLOW_SIM_SIMPLE_NODE_H
#define FLOW_SIM_SIMPLE_NODE_H
#include <cinttypes>
#include <vector>
#include <map>
#include <queue>
//#include "flowsim_config_macro.h"
#include <iostream>
#include "task.h"

struct SimpleNode {
    uint64_t hostid;
    // If we're dealing with spare matrices, then we need to consider a different representation 
    /*std::vector<double> direct_vector; // local traffic
    std::vector<std::vector<double>> buffer_matrix; // non-local traffic
    std::vector<double> proposals;   //index over mid nodes
    std::vector<double> acceptances; //index over src nodes
    std::vector<uint64_t> flow_list;*/

    std::queue<RWTask *> pending_tasks;
    std::map<uint64_t, RWTask *> running_tasks;
    std::map<uint64_t, Task *> finished_tasks;

    SimpleNode() {
    }

    SimpleNode(uint64_t id)
    :hostid(id){
    }

    ~SimpleNode() {
        for (auto &[task_id, task]: running_tasks){
            delete task;
        }

        for(auto &[task_id, task]: finished_tasks){
            delete task;
        }
    }
};

#endif