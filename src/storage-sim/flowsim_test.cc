#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <iostream>
#include <vector>

#include "simple_cluster.h"
#include "flowsim_config_macro.h"
#include "readfile_job.h"
#include "flowsim_topo_rotor.h"

int main(int argc, char *argv[]) {
    
    SimpleCluster cluster(RACK_COUNT, NODES_PER_RACK, LINK_SPEED, CORE_COUNT);
    //ISimulator *simulator = nullptr;
    ISimulator * simulator = new SingleLayerRotorSimulator(cluster);

    // uint64_t tor;
    // uint64_t data_size = Gb(100);
    // Flow flow1(0, tor, 7, tor, data_size, 0.00, nullptr);
    // //7 2 0 3 , 4 6 3 5 , 2 0 4 7, 3 4 1 0, 1 3 2 6, 6 5 7 1, 5 1 6 4, 0 7 5 2
    // flow1.setChannels(0,0);
    // Flow flow2(0, tor, 2, tor, data_size*2, 1.00, nullptr);
    // flow2.setChannels(1,0);
    // double time1 = simulator->TestFlows(flow1);
    // std::cout << "remaining time:" <<"\n";
    // std::cout << std::fixed << time1 <<"\n";
    //double time2 = simulator->TestFlows(flow2);
    //simulator->TestFlows(flow1);
    //simulator->TestFlows(flow2);

    HDFSDriver dataset(cluster, 1, FILE_SIZE);
    ReadFileJob job(dataset,simulator);
    job.Run();

    return 0;
}
