#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

//#include "flowsim_hadoop.h"
#include "simple_cluster.h"
#include "simple_job.h"
#include "hdfs_driver.h"
#include "flowsim_fattree.h"
#include "flowsim_rotorlb.h"
#include "flowsim_config_macro.h"
#include "flowsim_topo_test.h"

/*
static inline void print_usage_and_exit(char *argv0) {
    fprintf(stderr, "Usage: %s [fat_tree <k>|rotor_lb]\n", argv0);
    fprintf(stderr, "\tk: over-subscription factor\n");
    exit(EXIT_FAILURE);
}
*/

int main(int argc, char *argv[]) {
    /*if (argc < 2)
        print_usage_and_exit(argv[0]);
    */

    SimpleCluster cluster(RACK_COUNT, NODES_PER_RACK, LINK_SPEED, CORE_COUNT);
    ISimulator *simulator = nullptr;
    simulator = new TestTopologySimulator(cluster);
    
    /*if (strcmp(argv[1], "fat_tree") == 0) {
        if (argc < 3)
            print_usage_and_exit(argv[0]);
        int over_subscription = atoi(argv[2]);
        assert(over_subscription >= 1);
        simulator = new FatTreeSimulator(cluster, over_subscription);
    } else if (strcmp(argv[1], "rotor_lb") == 0)
        simulator = new RotorNetSimulator(cluster);
    else
        print_usage_and_exit(argv[0]);
    */
    HDFSDriver dataset(cluster, FILE_COUNT, FILE_SIZE);
    SimpleJob job(dataset, simulator);
    job.Run();

    return 0;
}
