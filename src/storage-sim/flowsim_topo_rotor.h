#ifndef FLOW_SIM_SINGLE_ROTOR_H
#define FLOW_SIM_SINGLE_ROTOR_H

#include <vector>
#include <numeric>
#include <set>
#include "link.h"
#include "simple_cluster.h"
#include "flowsim_sim_interface.h"
#include "flowsim_config_macro.h"
#include "util.h"

//#define TWO_HOP_PATH 0
#define ROTOR_LB 1
#define DEBUG 0

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



    #if ROTOR_LB
    //typedef std::tuple<uint64_t, uint64_t> uint64t_tuple; // here is <src, dst>
    // This vector does not bind to links, here we only care about the <src, dst> pair,
    // consider this case: we have two flows from node 0 to node 1,
    // seperated flow rate needs to be provided.
    // src-host <-> dst-host
    std::vector<std::vector<int>> flow_count_matrix;
    // key:<src, dst>, value: [flow_id 0 , flow_id1  ....]
    std::map< std::tuple<uint64_t, uint64_t>, std::vector<Flow> > flow_map; 
    // # of ToR * # of ToR
    //std::vector<std::vector<int>> direct_traffic_matrix;
    //std::vector<std::vector<int>> indirect_traffic_matrix;
    // flow_id -> rate at each topo slice
    std::map<uint64_t, std::vector<double>> deliver_rate_matrix;
    std::map<uint64_t, std::vector<double>> first_cycle_rate;
    #endif 

    //inherit these from ISimulator
    void InitializeLinks() override;
    uint64_t GetInterTorLinkId(uint64_t src_rack, uint64_t dst_rack) const;
    std::vector<link_id_t> GetLinkIds(const Flow &flow) const override;
    std::vector<link_id_t> GetInterTORLinkIds(uint64_t src_rack , uint64_t dst_rack) const;
    //void IncrementLinkFlowCount(const Flow &flow) override; // unused!
    double GetFlowRemainingTime(const Flow &flow) const override;
    uint64_t GetTransmittedBytes(const Flow &flow, double interval) const override;
    void UpdateLinkDemand() override;
    bool UpdateTwoHopLinkFlowCount(const Flow &flow);

    // our own rotor-specific methods as follow:
    void InitializeMatchingMatrix();
    void InitializePerTORMatrices();
    void InitializeFlowMatrices();
    //std::tuple<int, int> GetFlowChannel(const Flow &flow) const;
    std::tuple<int, int>  GetFlowChannel(uint64_t src_host, uint64_t dst_host, int slotnum) const;
    int GetFlowTORChannel(uint64_t src_rack, uint64_t dst_rack, int slotnum) const;
    std::vector<int> GetConnectedTORs(uint64_t dst_rack, int slice) const;
    double GetFlowRateForDownRotors(const Flow &flow) const;
    std::vector<double> RotorlbRatesCycle();
    std::vector<double> GetRatesPerCycle(const Flow &flow) const;
    std::vector<double> GetRatesForFirstCycle(const Flow &flow) const;
    std::vector<uint64_t> GetBytesPerCycle(const Flow &flow, std::vector <double> &rates) const;

    void RotorlbDirectPhase(std::vector<uint64_t> src_hosts, std::vector<uint64_t> dst_hosts, int slice);
    void RotorlbBufferPhase(std::vector<uint64_t> src_hosts, std::vector<uint64_t> dst_hosts, int slice);
    //-----------//
    void RotorBufferedTransfer(TORSwitch& mid_tor, TORSwitch& dst_tor, std::vector<double>& buffer_dest, int slice);
    void RotorDirectTransfer(TORSwitch& src_tor, TORSwitch& dst_tor, int slice);
    void RotorSourceProposal(int mid_tor, std::vector<int> src_tor_list, int slice);
    //-----------//
    void RotorCalculateAcceptance(int mid_tor, std::vector<int> src_tor_list, int slice);
    void RotorIndirection(TORSwitch& src_tor, TORSwitch& mid_tor, int slice);
    //----------------//
    double deliverTORFlows(TORSwitch& src_tor, std::vector<uint64_t> src_host, std::vector<uint64_t> dst_hosts, double tor_capacity, int channel, bool isLocalFlow, int slice);
    double updateTORFlows(TORSwitch& src_tor, TORSwitch& mid_tor, double agreed_capcity, int channel,int slice);
    bool HasIncomingFlows(uint64_t dst_rack){
        bool hasflow = false;
        for(uint32_t src_rack =0; src_rack < flow_count_matrix.size(); src_rack++){
            if(flow_count_matrix[src_rack][dst_rack] > 0){
                hasflow = true;
                break;
            }
        }    
        return hasflow;  
    }

    //if DEBUG
    void printVectorPerPhase(std::string phase){
        std::cout << "---------------"<< phase <<"---------------------\n";
        for(auto tor_iter = cluster.tors.begin(); tor_iter != cluster.tors.end(); ++tor_iter){
            uint64_t torid = tor_iter->first;
            TORSwitch& tor = tor_iter->second;    
            std::cout << "tor_id:" << +torid << "\n";
            util::printVector(tor.local_vector,  "local vector");
            util::printMatrix(tor.buffer_matrix, "buffer matrix");   
        }
    }
    //#endif 


public:
    void TestFlowsPerSlice(){
    }

    SingleLayerRotorSimulator();

    SingleLayerRotorSimulator(SimpleCluster cluster)
        : ISimulator(cluster),
        channel_count(HOST_CHANNELS),
        num_slots(ROTOR_SLOTS),
        cycle_time(DEFAULT_SLOT_TIME* ROTOR_SLOTS),
        filename(TOPO_FILENAME) 
        {
        printf("[Config] Test-Topology, %" PRIu64 " racks, %" PRIu64 " hosts/rack, %f Gbps link.\n",
              rack_count, hosts_per_rack, link_speed / Gb(1));
        //host_link_speed = link_speed;
        //num_slots = ROTOR_SLOTS;
        InitializeLinks();
        InitializeMatchingMatrix();
        #if ROTOR_LB
        for(size_t index = 0; index < cluster.tors.size(); index ++){  
            flow_count_matrix.push_back(std::vector<int>(cluster.tors.size()));
        }
        InitializePerTORMatrices();
        #endif
    }

    ~SingleLayerRotorSimulator(){
        for(int i = 0; i < num_slots; ++i) {
            delete [] rotor_matchings[i];
        }
        delete [] rotor_matchings;

    }
};


#endif 