#ifndef FLOW_SIM_GENERAL_CLUSTER_H
#define FLOW_SIM_GENERAL_CLUSTER_H
#include <vector>
#include <map>
#include <cinttypes>
#include "flowsim_config_macro.h"
#include "task.h"

struct SimpleNode {
    std::map<uint64_t, MapTask *> map_tasks;

    SimpleNode() {
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
    int core_count;
    std::map<uint64_t, SimpleNode> hosts;

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
                hosts[host_id] = SimpleNode();
            }
        }
    }

    uint64_t GetTotalNodeCount() const {
        return rack_count * nodes_per_rack;
    }

    uint64_t GetRackId(uint64_t node_id) const {
        return node_id / nodes_per_rack;
    }

    uint64_t GetHostId(uint64_t rack_id, uint64_t host_index) const {
        return rack_id * this->nodes_per_rack + host_index;
    }
};


#endif