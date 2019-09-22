#ifndef FLOW_SIM_GENERAL_CLUSTER_H
#define FLOW_SIM_GENERAL_CLUSTER_H
#include <vector>
#include <map>
#include <queue>
#include <cinttypes>
#include "flowsim_config_macro.h"
#include "simple_node.h"
#include "tor.h"
#include "task.h"


struct SimpleCluster {
    uint64_t rack_count;
    uint64_t nodes_per_rack;
    double link_speed;
    int core_count; // half for send, half for recv
    std::map<uint64_t, SimpleNode> hosts;
    std::map<uint64_t, TORSwitch> tors;

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
            tors[rack_id] = TORSwitch(rack_id);
        }
    }

    SimpleNode getNode(uint64_t id){
        return hosts[id];
    }

    TORSwitch getTOR(uint64_t id){
        return tors[id];
    }

    std::vector<SimpleNode> getNodesForTOR(uint64_t rack_id){
        std::vector<SimpleNode> vec(nodes_per_rack);
        for (uint64_t node_index = 0; node_index < nodes_per_rack; node_index++) {
            uint64_t host_id = GetHostId(rack_id, node_index);
            vec.push_back(hosts[host_id]);
        }
        return vec;
    }

    std::vector<uint64_t> getNodeIDsForTOR(uint64_t rack_id){
        std::vector<uint64_t> vec(nodes_per_rack);
        for (uint64_t node_index = 0; node_index < nodes_per_rack; node_index++) {
            uint64_t host_id = GetHostId(rack_id, node_index);
            vec.push_back(host_id);
        }
        return vec;

    }

    uint64_t GetTotalNodeCount() const {
        return rack_count * nodes_per_rack;
    }

    /*uint64_t GetTotalDatanodeCount() const {
        return rack_count * nodes_per_rack; 
    }*/

    uint64_t GetRackId(uint64_t node_id) const {
        return node_id / nodes_per_rack;
    }

    uint64_t GetHostId(uint64_t rack_id, uint64_t host_index) const {
        return rack_id * nodes_per_rack + host_index;
    }
};


#endif