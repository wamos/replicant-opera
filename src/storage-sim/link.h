#ifndef FLOW_SIM_LINK_H
#define FLOW_SIM_LINK_H
#include <tuple>
#include <vector>
#include <cstring>
#include <map>
#include "flowsim_config_macro.h"

struct Link {
    double link_speed;
    int flow_count; // # of flows always transmitting
    std::vector<int> flow_counts; // # of flows in TDMA channels
    int num_slot;
    int channel_count;
    double transmit_slot_time;
    //dobule link_speed

    
    // flow_id -> rate at each topo slice
    std::map<uint64_t, std::vector<double>> rate_map;
    // available capacity for the future flows
    std::vector<std::vector<double>> avail_capacity; //pre_slot-based

    // using std::vector for brevity. It should be changed to **ptr matrix as follow:
    // int **twohop_flow_counts;
    // twohop_flow_counts needs to be tracked in time slot based, so we can keep track of 
    // the number of flows belonging to an exact time slot
    // std::vector<std::vector<int>> twohopflow_counts; //(std::vector<int>(channel_count));
    std::vector<int> twohopflow_counts;
    std::vector<std::vector<int>> perslot_flowcount; 

    //Link(){};

    Link(double capacity, int channel_count = 0, int num_slot = 8)
        //: capacity(capacity), flow_count(0), flow_counts(std::vector<int>(static_cast<unsigned long>(channel_count))) {}
        : link_speed(capacity)
        ,num_slot(num_slot)
        ,channel_count(channel_count)
        ,flow_count(0)
        ,transmit_slot_time(DEFAULT_SLOT_TIME * DEFAULT_DUTY_CYCLE)
        ,twohopflow_counts(std::vector<int>(channel_count))
        ,flow_counts(std::vector<int>(channel_count)){ //,twohop_flow_counts(nullptr) {
        for(int slot = 0; slot < num_slot ; slot++){
            perslot_flowcount.push_back(std::vector<int>(channel_count));
            avail_capacity.push_back(std::vector<double>(channel_count));
        }
        //InitializeFlowCountMatrix(capacity, channel_count, num_slot);
    }

    double getAvailCapacity(int channel, int slot){
        return avail_capacity[slot][channel];
    }

    std::vector<double> getCapacityVector(int slot){
        return avail_capacity[slot];
    }

    void ReduceAvailCapacity(int channel, int slot, double value){
        avail_capacity[slot][channel] = avail_capacity[slot][channel] - value;
    }

    void ResetFlowStats() {
        for(int slot = 0; slot < num_slot ; slot++){
            std:fill(avail_capacity[slot].begin(), avail_capacity[slot].end(), transmit_slot_time*link_speed);
        }
        rate_map.clear();
    }

    double GetRatePerFlow(int channel, int slot) const {
        double total_flow_count = 0;
        if (channel >= 0){
            //total_flow_count = flow_counts[channel];
            total_flow_count = perslot_flowcount[slot][channel];
            //std::cout << "channel:" << channel << ", total flow count:"<< total_flow_count << "\n";
        }
        else{ //channel = -1
          //the current flow doesn't have a proper channel, flow cannot have any rate
          //std::cout << "invalid channel:"<< channel << "\n";
        }
        if (total_flow_count == 0)
            return 0;
        else
            return link_speed / total_flow_count;
    }


    double GetRatePerTwohopFlow(int channel, int slot) const {
        double total_flow_count = 0;
        if (channel >= 0){
            //total_flow_count = flow_counts[channel];
            //total_flow_count  = twohopflow_counts[channel];
            total_flow_count = perslot_flowcount[slot][channel];
            //std::cout << "channel:" << channel << ", total flow count:"<< total_flow_count << "\n";
        }
        if (total_flow_count == 0)
            return 0;
        else
            return link_speed / total_flow_count;
    }

    
    void ResetFlowCounts() {
        flow_count = 0;
        fill(flow_counts.begin(), flow_counts.end(), 0);
        fill(twohopflow_counts.begin(), twohopflow_counts.end(), 0); 
        for(int slot = 0; slot < num_slot ; slot++){
            auto& vec =perslot_flowcount[slot];
            fill(vec.begin(), vec.end(), 0);
        }    

        //TODO: set all entries in twohop_flow_counts to zero
        /*for(int i = 0; i < num_slot; ++i) {
            std::memset(twohop_flow_counts[i], 0, sizeof(twohop_flow_counts[i]));
        }*/    
    }

    void IncrementFlowCount() {
        flow_count++;
    } 

    void IncrementFlowCount(int channel, int slot) {
        if(channel >=0 && slot >=0 ){
            perslot_flowcount[slot][channel]++;
        }
    } 

    void IncrementTwohopFlowCount(int channel, int slot) {
        if(channel >=0){
            perslot_flowcount[slot][channel]++;
            //flow_counts[channel]++;
            //twohopflow_counts[channel]++;
        }
    }

    void IncrementFlowCount(int channel) {
        //flow_counts[static_cast<unsigned long>(channel)]++;
        if(channel >=0){
            flow_counts[channel]++;
        }
    }
};

#endif //FLOW_SIM_LINK_H
