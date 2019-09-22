#ifndef FLOW_SIM_SIMPLE_NODE_H
#define FLOW_SIM_SIMPLE_NODE_H
#include <cinttypes>
#include <vector>
#include <map>
#include <queue>
#include "flowsim_config_macro.h"
#include "task.h"

struct SimpleNode {
    uint64_t hostid;
    // If we're dealing with spare matrices, then we need to consider a different representation 
    std::vector<double> direct_vector; // local traffic
    std::vector<std::vector<double>> buffer_matrix; // non-local traffic
    std::vector<double> proposals;   //index over mid nodes
    std::vector<double> acceptances; //index over src nodes
    std::vector<uint64_t> flow_list;

    std::queue<RWTask *> pending_tasks;
    std::map<uint64_t, RWTask *> running_tasks;
    std::map<uint64_t, Task *> finished_tasks;

    SimpleNode() {
    }

    SimpleNode(uint64_t id)
    :hostid(id){
    }

    void InitFlowMatrices(int num_host){
        for(int host = 0; host < num_host ; host++){
            buffer_matrix.push_back(std::vector<double>(num_host));
            std::fill(buffer_matrix[host].begin(), buffer_matrix[host].end(), 0.0);
            direct_vector.push_back(0.0);
            proposals.push_back(0.0);
            acceptances.push_back(0.0);
        }
    }

    void addFlowtoNode(uint64_t flow_id){
        flow_list.push_back(flow_id);
    }

    std::vector<uint64_t>& getFlowList(){
        return flow_list;
    }

    double getBufferValue(int row, int col){
        return buffer_matrix[row][col];
    }

    std::vector<double>& getDirectVector(){
        return direct_vector;
    }
    
    double getDirectValue(int dst){
        return direct_vector[dst];
    }

    bool updateBufferValue(int row, int col, double val){
        buffer_matrix[row][col] = val;
        return true;
    }

    void updateDirectValue(int dst, double val){
        direct_vector[dst] = val;
    }

    void printDirectVector(){
        for(auto e: direct_vector){
            std::cout << e << ",";
        }
        std::cout << "\n";
    }

    void printBufferMatrix(){
        for(int i = 0 ; i < buffer_matrix.size(); i++){
            for(int j = 0; j < buffer_matrix[0].size();j ++){
                std::cout << buffer_matrix[i][j] << ",";
            }
            std::cout << "\n";
        }
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