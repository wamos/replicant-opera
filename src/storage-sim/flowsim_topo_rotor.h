#ifndef FLOW_SIM_SINGLE_ROTOR_H
#define FLOW_SIM_SINGLE_ROTOR_H

#include <vector>
#include <set>
#include "link.h"
#include "simple_cluster.h"
#include "flowsim_sim_interface.h"
#include "flowsim_config_macro.h"
#define TWO_HOP_PATH 1
#define BUFFERLESS_TWO_HOP 0
#define ROTOR_LB_TWO_HOP 1

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

    #if ROTOR_LB_TWO_HOP
    // this vector is for store and forward
    // dsts can be reached in two-hop, dst -> vector of mid_hosts
    std::vector<std::map<uint64_t, std::set<uint64_t>>> rotorlb_midlist; 
    #endif

    #if TWO_HOP_PATH
    // typedef std::map<uint64_t, std::set<uint64_t>> map_set;//dst_midlist; 
    // this is for per-slot based two-hop paths, dsts can be reached in two-hop, key:dst -> value: vector of mid_hosts
    std::vector<std::map<uint64_t, std::set<uint64_t>>> dst_midlist; 
    #endif

    //inherit these from ISimulator
    void InitializeLinks() override;
    std::vector<link_id_t> GetLinkIds(const Flow &flow) const override;
    void IncrementLinkFlowCount(const Flow &flow) override;
    double GetFlowRemainingTime(const Flow &flow) const override;
    uint64_t GetTransmittedBytes(const Flow &flow, double interval) const override;
    void UpdateLinkDemand() override;
    bool UpdateTwoHopLinkFlowCount(const Flow &flow);
    //void TwoHopLinkUpdate(int slotnum);

    // our own rotor-specific methods as follow:
    void InitializeMatchingMatrix();
    //std::tuple<int, int> GetFlowChannel(const Flow &flow) const;
    std::tuple<int, int>  GetFlowChannel(uint64_t src_host, uint64_t dst_host, int slotnum) const;
    double GetFlowRateForDownRotors(const Flow &flow) const;
    std::vector<double> GetRatesPerCycle(const Flow &flow) const;
    //std::vector<double> GetRatesPerChannel(const Flow &flow) const;
    std::vector<uint64_t> GetBytesPerCycle(const Flow &flow, std::vector <double> &rates) const;
    //std::vector<uint64_t> GetBytesPerChannel(const Flow &flow, std::vector <double> &rates) const;

public:

    /*double TestFlows(const Flow& flow){
        UpdateLinkDemand();
        //InitializeLinks(); //if c'tor is not called
        for(int slot = 0; slot < num_slots; slot++){
            TwoHopLinkUpdate(slot, flow);
        }
        std::vector vec = GetRatesPerCycle(flow);
        std::cout << "-----------------------\n";
        std::cout << "GetRatesPerCycle\n";
        for(auto rate: vec){
           std::cout << rate <<",";     
        }
        std::cout << "\n";
        //double time = GetFlowRemainingTime(flow);
        return 0.0;
    }*/

    SingleLayerRotorSimulator();

    SingleLayerRotorSimulator(SimpleCluster cluster)
        : ISimulator(cluster),
        channel_count(HOST_CHANNELS),
        #if TWO_HOP_PATH
        rotorlb_midlist(std::vector<std::map<uint64_t, std::set<uint64_t>>>(num_slots)),
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
    }

    ~SingleLayerRotorSimulator(){
        for(int i = 0; i < num_slots; ++i) {
            delete [] rotor_matchings[i];
        }
        delete [] rotor_matchings;

    }
};


#endif //FLOW_SIM_SINGLE_ROTOR_H