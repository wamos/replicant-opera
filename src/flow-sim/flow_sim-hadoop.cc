#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "flow_sim-hadoop.h"
#include "flow_sim-fat_tree.h"
#include "flow_sim-rotor_lb.h"

static uint64_t RACK_COUNT = 256;
static uint64_t NODES_PER_RACK = 8;
static double LINK_SPEED = Gb(10);

static int MAP_CORE_COUNT = 8;
static int REDUCE_CORE_COUNT = 1;

static uint64_t FILE_COUNT = 2 * 1024;
static uint64_t FILE_SIZE = GB(40);

static inline void print_usage_and_exit(char *argv0) {
    fprintf(stderr, "Usage: %s [fat_tree <k>|rotor_lb]\n", argv0);
    fprintf(stderr, "\tk: over-subscription factor\n");
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
    if (argc < 2)
        print_usage_and_exit(argv[0]);

    HadoopCluster cluster(RACK_COUNT, NODES_PER_RACK, LINK_SPEED, MAP_CORE_COUNT, REDUCE_CORE_COUNT);
    ISimulator *simulator = nullptr;
    if (strcmp(argv[1], "fat_tree") == 0) {
        if (argc < 3)
            print_usage_and_exit(argv[0]);
        int over_subscription = atoi(argv[2]);
        assert(over_subscription >= 1);
        simulator = new FatTreeSimulator(cluster, over_subscription);
    } else if (strcmp(argv[1], "rotor_lb") == 0)
        simulator = new RotorNetSimulator(cluster);
    else
        print_usage_and_exit(argv[0]);

    HadoopDataset dataset(cluster, FILE_COUNT, FILE_SIZE);
    HadoopSort hadoopSort(dataset, simulator);

    hadoopSort.Run();

    return 0;
}
