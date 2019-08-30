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
    SingleLayerRotorSimulator* simulator = new SingleLayerRotorSimulator(cluster);

    //uint64_t tor;
    //uint64_t data_size = Gb(100);
    //7 2 0 3 , 4 6 3 5 , 2 0 4 7, 3 4 1 0, 1 3 2 6, 6 5 7 1, 5 1 6 4, 0 7 5 2
    //Flow flow0(0, tor, 1, tor, data_size, 0.00, nullptr);

    //flow0.setChannels(0,0);
    //simulator->TestFlows(flow0);
    //Flow flow1(2, tor, 4, tor, data_size, 0.00, nullptr);
    //simulator->TestFlows(flow1);

    // Flow flow2(0, tor, 2, tor, data_size*2, 1.00, nullptr);
    // flow2.setChannels(1,0);
    // double time1 = simulator->TestFlows(flow1);
    // std::cout << "remaining time:" <<"\n";
    // std::cout << std::fixed << time1 <<"\n";
    //double time2 = simulator->TestFlows(flow2);
    //simulator->TestFlows(flow0);
    //simulator->TestFlows(flow2);

    /*std::vector<double> cap0 = {3.0, 4.0, 6.0};
    std::vector<double> cap1 = {3.0, 3.0, 5.0};

    std::vector<int> cap0_int = {5, 5, 5 };
    std::vector<int> cap1_int = {4, 4, 4};
    //double cap = 3.0;
    std::vector<int> vec0_int = {3, 2, 1, 2};
    std::vector<double> vec0_double = {3.0, 2.0, 1.0, 2.0};

    std::vector<std::vector<int>> vec1_int= { {1, 2, 3}, {2, 1, 3}, {3, 1, 2} };
    std::vector<std::vector<double>> vec1_double= { {1.0, 2.0, 3.0}, {2.0, 1.0, 3.0}, {3.0, 1.0, 2.0} };
    
    auto sent = util::fairshare1d(vec0_double, 7.0, true); //expect 2.0, 2.0, 1.0, 2.0
    std::cout<< "sent_double\n";
    for(int i = 0; i < sent.size(); i++){
        std::cout<< sent[i] << ",";    
    }
    std::cout<< "\n";

    auto sent2 = util::fairshare2d(vec1_double, cap0, cap1);
    std::cout<< "\nsent 2d_2\n";
    for(int i = 0; i < sent2.size(); i++){
        for(int j = 0; j< sent2[i].size(); j++){
            std::cout << sent2[i][j] << ",";    
        }
        std::cout<< "\n";
    }
    std::cout<< "\n";*/
    //RWTask *read_req_0 = new RWTask(client.hostid, server1.hostid, 0, file_id, block_id, "1stFlow");
    //auto f = simulator->AddFlow(0, 1, FILE_SIZE, 0, nullptr);
    //simulator->TestFlowsPerSlice();
    HDFSDriver dataset(cluster, 1, FILE_SIZE);
    ReadFileJob job(dataset,simulator);
    job.Run();

    return 0;
}
