#ifndef FLOW_SIM_HDFS_MAIN_H
#define FLOW_SIM_HDFS_MAIN_H

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

struct HadoopDataset {
    static const uint64_t DEFAULT_BLOCK_SIZE = (uint64_t)5 * 1024 * 1024 * 1024;
    static const int DEFAULT_REPLICA_COUNT = 3;

    HadoopCluster cluster;
    uint64_t file_count;
    uint64_t file_size;
    uint64_t block_size;
    int replica_count;

    HadoopDataset(HadoopCluster cluster, uint64_t file_count, uint64_t file_size,
                    uint64_t block_size = DEFAULT_BLOCK_SIZE,
                    int replica_count = DEFAULT_REPLICA_COUNT)
        : cluster(cluster),
          file_count(file_count),
          file_size(file_size),
          block_size(block_size),
          replica_count(replica_count)
    {
        printf("[Config] HadoopDataset: %" PRIu64 " files, %" PRIu64 " GB/file, %" PRIu64 " MB/block, %d replicas.\n",
                file_count, file_size / GB(1), block_size / MB(1), replica_count);
    }

    uint64_t GetBlockCount() {
        return (uint64_t)ceil((double)this->file_size / (double)this->block_size);
    }

    uint64_t GetNodeForBlock(uint64_t file_id, uint64_t block_id = 0, int replica_id = 0) {
        if (file_id < 0 || file_id >= this->file_count)
            throw "file_id out of range";
        if (block_id < 0 or block_id >= this->GetBlockCount())
            throw "block_id out of range";
        if (replica_id < 0 || replica_id >= this->replica_count)
            throw "replica_id out of range";

        // Default placement per file, i.e. first replica.
        hash<uint64_t> hash_u64;
        auto default_host_id = hash_u64(file_id) % this->cluster.GetTotalNodeCount();
        if (replica_id == 0) {
            return default_host_id;
        } else {
            auto default_rack_id = cluster.GetRackId(default_host_id);
            auto rack_id = (hash_u64(file_id) ^ hash_u64(block_id)) % (cluster.rack_count - 1);
            rack_id = rack_id < default_rack_id ? rack_id : (rack_id + 1);
            auto host_index = (hash_u64(file_id) ^ hash_u64(block_id) ^ hash_u64((uint64_t)replica_id))
                                % cluster.nodes_per_rack;
            return cluster.GetHostId(rack_id, host_index);
        }
    }
};

#endif
