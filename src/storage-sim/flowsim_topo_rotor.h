#ifndef FLOW_SIM_SINGLE_ROTOR_H
#define FLOW_SIM_SINGLE_ROTOR_H

#include <vector>
#include <set>
#include "link.h"
#include "simple_cluster.h"
#include "flowsim_sim_interface.h"
#include "flowsim_config_macro.h"
#include "util.h"

#define TWO_HOP_PATH 0
#define ROTOR_LB 1
#define RESTRICTED_ROTORLB 0

class SingleLayerRotorSimulator : public ISimulator {
private:

    static constexpr double transmit_slot_time = DEFAULT_SLOT_TIME * DEFAULT_DUTY_CYCLE;
    static constexpr double duty_cycle = DEFAULT_DUTY_CYCLE;
    static constexpr double total_slot_time = DEFAULT_SLOT_TIME;
    const int channel_count;
    const double cycle_time;
    int **rotor_matchings;
    int num_slots;
    std::string filename;

    #if TWO_HOP_PATH
    // typedef std::map<uint64_t, std::set<uint64_t>> map_set;//dst_midlist; 
    // this is for per-slot based two-hop paths, dsts can be reached in two-hop, key:dst -> value: vector of mid_hosts
    std::vector<std::map<uint64_t, std::set<uint64_t>>> dst_midlist; 
    #endif


    #if ROTOR_LB
    // This vector does not bind to links, here we only care about the <src, dst> pair,
    // consider this case: we have two flows from node 0 to node 1,
    // seperated flow rate needs to be provided.
    // TODO: needs to init so we can update the matrix when AddFlow() is called
    std::vector<std::vector<int>> flow_count_matrix; 
    // flow_id -> rate at each topo slice
    std::map<uint64_t, std::vector<double>> deliver_rate_matrix;
    std::map<uint64_t, std::vector<double>> first_cycle_rate;
    #endif 

    //inherit these from ISimulator
    void InitializeLinks() override;
    uint64_t GetInterTorLinkId(uint64_t src_rack, uint64_t dst_rack) const;
    std::vector<link_id_t> GetLinkIds(const Flow &flow) const override;
    void IncrementLinkFlowCount(const Flow &flow) override;
    double GetFlowRemainingTime(const Flow &flow) const override;
    uint64_t GetTransmittedBytes(const Flow &flow, double interval) const override;
    void UpdateLinkDemand() override;
    bool UpdateTwoHopLinkFlowCount(const Flow &flow);
    //void TwoHopLinkUpdate(int slotnum);

    // our own rotor-specific methods as follow:
    void InitializeMatchingMatrix();
    void InitializePerNodeMatrices();
    //std::tuple<int, int> GetFlowChannel(const Flow &flow) const;
    std::tuple<int, int>  GetFlowChannel(uint64_t src_host, uint64_t dst_host, int slotnum) const;
    double GetFlowRateForDownRotors(const Flow &flow) const;
    std::vector<double> RotorlbRatesCycle();
    void RotorlbDirectPhase(std::vector<uint64_t> src_hosts, std::vector<uint64_t> dst_hosts, int slice);
    void RotorlbBufferPhase(std::vector<uint64_t> src_hosts, std::vector<uint64_t> dst_hosts, int slice);
    std::vector<double> GetRatesPerCycle(const Flow &flow) const;
    std::vector<double> GetRatesForFirstCycle(const Flow &flow) const;
    std::vector<uint64_t> GetBytesPerCycle(const Flow &flow, std::vector <double> &rates) const;



public:
    void TestFlowsPerSlice(){
        //UpdateLinkDemand():
        for(auto map_iter = links.begin(); map_iter != links.end(); ++map_iter){
            map_iter->second.ResetFlowStats();
        }

        for(int index = 0; index < cluster.hosts.size(); index ++){ 
                std::fill(flow_count_matrix[index].begin(), flow_count_matrix[index].end(), 0);
        }

        std::vector<uint64_t> src_hosts;
        std::vector<uint64_t> dst_hosts;

        for (const auto &flow: flows) {
            if (!flow.HasStarted(time_now) || flow.IsCompleted()){
                continue;
            }
            src_hosts.push_back(flow.src_host);
            dst_hosts.push_back(flow.dst_host);
            flow_count_matrix[flow.src_host][flow.dst_host]++;
            auto vec = std::vector<double>(num_slots);
            std::fill(vec. begin(),vec.end(), 0.0);
            deliver_rate_matrix[flow.flow_id] = vec;
            first_cycle_rate[flow.flow_id] = vec;
        }

        // to warm up the first slice, we need to do num_slots+1 
        for(int slice = 0; slice < num_slots*2; slice++){
            if(slice == num_slots){
                for(auto map_iter = links.begin(); map_iter != links.end(); ++map_iter){
                map_iter->second.ResetFlowStats();
                }
            }
            std::cout<< "------slice:"<< slice%num_slots <<"-----\n";
            RotorlbDirectPhase(src_hosts, dst_hosts, slice);
            /*if(slice == 0){
                for (const auto &flow: flows) {
                    auto matrix = deliver_rate_matrix[flow.flow_id];
                    first_slice_rate[flow.flow_id] = matrix[slice];
                }
            }*/
            //check deliver rate matrix
            //std::cout<< "------after phase 1------\n";
            /*if(slice > 0){
                std::cout<< "deliver rate matrix:\n";
                for (const auto &flow: flows) {
                    auto matrix = deliver_rate_matrix[flow.flow_id];
                    std::cout << matrix[slice%num_slots] << "\n";
                }
            }*/
            //std::cout<< "deliver rate matrix ends\n";
            //check proposal matrix
            /*std::cout<< "proposal matrix\n";
            for(int i = 0 ; i < proposals.size(); i++){
                for(int j = 0; j < proposals[0].size();j ++){
                    std::cout << proposals[i][j] << ",";
                }
                std::cout << "\n";
            }*/
            // for all nodes
            /*
            std::cout<< "all nodes\n";
            for(auto map_iter = cluster.hosts.begin(); map_iter != cluster.hosts.end(); ++map_iter){ // for all nodes
                uint64_t id = map_iter->first;
                SimpleNode& node = cluster.hosts[id];
                std::cout<< "node" << +id << ":\n";
                node.printDirectVector();
                node.printBufferMatrix();
            }*/
            RotorlbBufferPhase(src_hosts, dst_hosts, slice%num_slots);
            //std::cout<< "------phase 2 and 3 done------\n";
            //check acceptance matrix
            /*std::cout<< "acceptance matrix\n";
            for(int i = 0 ; i < acceptances.size(); i++){
                for(int j = 0; j < acceptances[0].size();j ++){
                    std::cout << acceptances[i][j] << ",";
                }
                std::cout << "\n";
            }*/
            //for all nodes
            /*std::cout<< "all nodes\n";
            for(auto map_iter = cluster.hosts.begin(); map_iter != cluster.hosts.end(); ++map_iter){ // for all nodes
                uint64_t id = map_iter->first;
                std::cout<< "node "<< +id << "\n";
                SimpleNode& node = cluster.hosts[id];
                //node.printDirectVector();
                node.printBufferMatrix();
            }*/
        }

        for (const auto &flow: flows) {
            auto matrix = deliver_rate_matrix[flow.flow_id];
            std::cout<< "flow:"<<  +flow.src_host << "->"<< +flow.dst_host <<"\n";
            double unit_100gbps = 0;
            for(int slice = 0; slice < num_slots; slice++){
                //std::cout << matrix[slice%num_slots]/12500000000.0 << "\n";
                unit_100gbps += matrix[slice%num_slots]/12500000000.0;        
            }
            std::cout << "num of 100 gbps units:" << unit_100gbps << "\n";
            //std::cout << "first cycle rate matrix:\n";
            //for(int slice = 0; slice < num_slots; slice++){
            //std::cout << std::fixed << first_cycle_rate[flow.flow_id] << "\n";
        }


    }

    SingleLayerRotorSimulator();

    SingleLayerRotorSimulator(SimpleCluster cluster)
        : ISimulator(cluster),
        channel_count(HOST_CHANNELS),
        #if ROTOR_LB
        // TODO init flow_count_matrix and deliver_rate_matrix
        #elif TWO_HOP_PATH
        dst_midlist(std::vector<std::map<uint64_t, std::set<uint64_t>>>(num_slots)),
        #endif
        num_slots(ROTOR_SLOTS),
        cycle_time(DEFAULT_SLOT_TIME* ROTOR_SLOTS),
        filename(TOPO_FILENAME) 
        {
        printf("[Config] Test-Topology, %" PRIu64 " racks, %" PRIu64 " hosts/rack, %f Gbps link.\n",
              rack_count, hosts_per_rack, link_speed / Gb(1));
        //host_link_speed = link_speed;
        num_slots = ROTOR_SLOTS;
        InitializeLinks();
        InitializeMatchingMatrix();
        for(int index = 0; index < cluster.hosts.size(); index ++){  
            //proposals.push_back(std::vector<double>(cluster.hosts.size()));
            //acceptances.push_back(std::vector<double>(cluster.hosts.size()));
            flow_count_matrix.push_back(std::vector<int>(cluster.hosts.size()));
        }   
        //for(int slot = 0; slot < hosts_per_rack ; slot++){
            //flow_count_matrix.push_back(std::vector<int>(hosts_per_rack));
            //proposals.push_back(std::vector<double>(hosts_per_rack));
            //acceptances.push_back(std::vector<double>(hosts_per_rack));
        //}
        InitializePerNodeMatrices();
    }

    ~SingleLayerRotorSimulator(){
        for(int i = 0; i < num_slots; ++i) {
            delete [] rotor_matchings[i];
        }
        delete [] rotor_matchings;

    }
};


#endif //FLOW_SIM_SINGLE_ROTOR_H