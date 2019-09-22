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

struct HDFSDriver {
    
    SimpleCluster cluster;
    uint64_t file_count;
    uint64_t file_size;
    uint64_t block_size;
    std::map<uint64_t, std::vector<uint64_t>> blocks_to_hosts; // replicated blocks: hosts_0, hosts_1 
    std::map<uint64_t, std::vector<uint64_t>> file_to_blocks; // file_id: block_id_0, block_id_1
    int replica_count;

    HDFSDriver(){};

    HDFSDriver(SimpleCluster cluster, uint64_t file_count, uint64_t file_size,
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

    void updateBlockList( uint64_t file_id, std::vector<uint64_t> block_list){
        auto& vec = file_to_blocks[file_id];
        auto it = vec.end();
        vec.insert(it, block_list.begin(), block_list.end());
    }

    std::vector<uint64_t> getBlockList(uint64_t file_id){
        return file_to_blocks[file_id];
    }

    std::vector<uint64_t> getBlockLocations(uint64_t block_id){
        return blocks_to_hosts[block_id];
    }

    uint64_t placeBlocks(uint64_t file_id, uint64_t block_id, int replica_id){
        std::hash<uint64_t> hash_u64; //TODO: this is a idiot hash, will be need to replaced
        uint64_t block_hash = hash_u64(file_id) ^ hash_u64(block_id) ^ hash_u64(replica_id);
        uint64_t block_location = (block_hash % cluster.GetTotalNodeCount()); 
        std::cout << "file_id, block_id, block loc: " <<  +file_id << +block_id << +block_location << "\n";
        auto& vec = blocks_to_hosts[block_id];
        auto it = vec.end();
        vec.insert(it, block_location);
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
        auto default_host_id = hash_u64(file_id) % cluster.GetTotalNodeCount();
        if (replica_id == 0) {
            return default_host_id;
        }
         
        
        /*else {
            auto default_rack_id = cluster.GetRackId(default_host_id);
            auto rack_id = (hash_u64(file_id) ^ hash_u64(block_id)) % (cluster.rack_count - 1);
            rack_id = rack_id < default_rack_id ? rack_id : (rack_id + 1);
            auto host_index = (hash_u64(file_id) ^ hash_u64(block_id) ^ hash_u64((uint64_t)replica_id))
                                % cluster.nodes_per_rack;
            return cluster.GetHostId(rack_id, host_index);
        }*/
    }
};

#endif
