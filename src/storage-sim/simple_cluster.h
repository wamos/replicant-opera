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
    std::vector<std::vector<int>> nonlocal_buffer_matrix; 
    std::vector<uint64_t> flow_list;
    
    std::map<uint64_t, Task *> map_tasks;
    std::queue<RWTask *> pending_tasks;
    //std::queue<uint64_t> pending_queue;
    std::map<uint64_t, RWTask *> running_tasks;
    std::map<uint64_t, Task *> finished_tasks;
    std::map<uint64_t, Task *> completed_tasks;

    SimpleNode() {
    }

    SimpleNode(uint64_t id):hostid(id){
    }

    ~SimpleNode() {
        for (auto &[task_id, task]: map_tasks)
            delete task;
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
                hosts[host_id] = SimpleNode(host_id);
            }
        }
    }

    /*void ConfigSingleRack(){
        rack_count=1;
        for (uint64_t rack_id = 0; rack_id < rack_count; rack_id++) {
            //node index = 0 is used as a client
            //node index = 1 is used as a namenode
            for (uint64_t node_index = 2; node_index < nodes_per_rack; node_index++) {
                uint64_t host_id = GetHostId(rack_id, node_index);
                hosts[host_id] = SimpleNode();
            }
        }
    }*/

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