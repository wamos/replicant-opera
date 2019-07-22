#ifndef FLOW_SIM_CONFIG_H
#define FLOW_SIM_CONFIG_H

#include <tuple>

#define KB(n) ((uint64_t)n * 1024)
#define MB(n) ((uint64_t)n * 1024 * 1024)
#define GB(n) ((uint64_t)n * 1024 * 1024 * 1024)
#define Kb(n) ((uint64_t)n * 1024 / 8)
#define Mb(n) ((uint64_t)n * 1024 * 1024 / 8)
#define Gb(n) ((uint64_t)n * 1024 * 1024 * 1024 / 8)

enum LinkType { HOST_TOR, TOR_CORE, CORE_TOR, TOR_HOST, TOR_TOR, HOST_HOST };
typedef std::tuple<LinkType, uint64_t> link_id_t;

static uint64_t RACK_COUNT = 256;
static uint64_t NODES_PER_RACK = 8;
static double LINK_SPEED = Gb(10);

static int MAP_CORE_COUNT = 8;
static int REDUCE_CORE_COUNT = 1;

static uint64_t FILE_COUNT = 2 * 1024;
static uint64_t FILE_SIZE = GB(40);

#endif //FLOW_SIM_CONFIG_H