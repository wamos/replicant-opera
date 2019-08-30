#ifndef FLOW_SIM_GENERAL_CLUSTER_H
#define FLOW_SIM_GENERAL_CLUSTER_H
#include <vector>
#include <map>
#include <queue>
#include <vector>
#include <cinttypes>
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
    //std::map<uint64_t, Task *> map_tasks;
    //std::queue<uint64_t> pending_queue;
    //std::map<uint64_t, Task *> completed_tasks;

    SimpleNode() {
    }

    SimpleNode(uint64_t id)//, double buf_lim)
    :hostid(id){
    //buffer_limit(buf_lim){
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

struct SimpleCluster {
    uint64_t rack_count;
    uint64_t nodes_per_rack;
    double link_speed;
    int core_count; // half for send, half for recv
    std::map<uint64_t, SimpleNode> hosts;

    SimpleCluster(){};

    SimpleCluster(uint64_t rack_count, uint64_t nodes_per_rack, double link_speed, int core_count)
            : rack_count(rack_count),
              nodes_per_rack(nodes_per_rack),
              link_speed(link_speed),
              core_count(core_count) {
        printf("[Config] Cluster %" PRIu64 " racks, %" PRIu64 " hosts/rack, %f Gbps link, %d cores/host.\n",
               rack_count, nodes_per_rack, link_speed / Gb(1), core_count);
        for (uint64_t rack_id = 0; rack_id < rack_count; rack_id++) {
            for (uint64_t node_index = 0; node_index < nodes_per_rack; node_index++) {
                uint64_t host_id = GetHostId(rack_id, node_index);
                hosts[host_id] = SimpleNode(host_id); //buf_lim);
            }
        }
    }

    SimpleNode getNode(uint64_t id){
        return hosts[id];
    }

    uint64_t GetTotalNodeCount() const {
        return rack_count * nodes_per_rack;
    }

    uint64_t GetTotalDatanodeCount() const {
        //return rack_count * nodes_per_rack - 2; // one for client node 0, one for namenode node 1; 
        return rack_count * nodes_per_rack; // one for client node 0, one for namenode node 1; 
    }

    uint64_t GetRackId(uint64_t node_id) const {
        return node_id / nodes_per_rack;
    }

    uint64_t GetHostId(uint64_t rack_id, uint64_t host_index) const {
        return rack_id * this->nodes_per_rack + host_index;
    }
};


#endif