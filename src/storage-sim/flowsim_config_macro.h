#ifndef FLOW_SIM_CONFIG_H
#define FLOW_SIM_CONFIG_H

#include <tuple>

#define KB(n) ((uint64_t)n * 1024)
#define MB(n) ((uint64_t)n * 1024 * 1024)
#define GB(n) ((uint64_t)n * 1024 * 1024 * 1024)
#define Kb(n) ((uint64_t)n * 1024 / 8)
#define Mb(n) ((uint64_t)n * 1024 * 1024 / 8)
#define Gb(n) ((uint64_t)n * 1024 * 1024 * 1024 / 8)

enum LinkType { HOST_TOR, TOR_CORE, CORE_TOR, TOR_HOST, TOR_TOR };
typedef std::tuple<LinkType, uint64_t> link_id_t;

static const uint64_t RACK_COUNT = 1;
static const uint64_t NODES_PER_RACK = 10;
static const double LINK_SPEED = Gb(100);

static const int CORE_COUNT = 16;
//static int MAP_CORE_COUNT = 8;
//static int REDUCE_CORE_COUNT = 1;

static const double DEFAULT_DUTY_CYCLE = 0.9;
static const double DEFAULT_SLOT_TIME = 200e-6;
static const int ROTOR_CHANNEL = 4;
// each host has four 100 Gbps ports. 
// e.g. Two dual-port ConnectX-5 NICs on a single host

static const uint64_t FILE_COUNT = 16;
static const uint64_t FILE_SIZE = GB(40);
static const uint64_t METADATA_SIZE = 640*150; // num of blocks in 40GB file * metadata_size_per_block

static const uint64_t DEFAULT_BLOCK_SIZE = MB(64);
static const int DEFAULT_REPLICA_COUNT = 3;

#endif //FLOW_SIM_CONFIG_H