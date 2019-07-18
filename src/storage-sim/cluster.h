struct HadoopNode {
    map<uint64_t, MapTask *> map_tasks;
    map<uint64_t, ReduceTask *> reduce_tasks;
    map<uint64_t, Task *> completed_tasks;

    HadoopNode() {
    }

    ~HadoopNode() {
        for (auto &[task_id, task]: map_tasks)
            delete task;
        for (auto &[task_id, task]: reduce_tasks)
            delete task;
    }
};

struct HadoopCluster {
    uint64_t rack_count;
    uint64_t nodes_per_rack;
    double link_speed;
    int map_core_count;
    int reduce_core_count;
    map<uint64_t, HadoopNode> hosts;

    HadoopCluster(uint64_t rack_count, uint64_t nodes_per_rack, double link_speed, int map_core_count, int reduce_core_count)
            : rack_count(rack_count),
              nodes_per_rack(nodes_per_rack),
              link_speed(link_speed),
              map_core_count(map_core_count),
              reduce_core_count(reduce_core_count) {
        printf("[Config] Cluster %" PRIu64 " racks, %" PRIu64 " hosts/rack, %f Gbps link, %d maps/host, %d reduces/host.\n",
               rack_count, nodes_per_rack, link_speed / Gb(1), map_core_count, reduce_core_count);
        for (uint64_t rack_id = 0; rack_id < rack_count; rack_id++) {
            for (uint64_t node_index = 0; node_index < nodes_per_rack; node_index++) {
                uint64_t host_id = GetHostId(rack_id, node_index);
                hosts[host_id] = HadoopNode();
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
