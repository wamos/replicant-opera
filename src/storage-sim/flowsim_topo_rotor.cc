#include <utility>
#include <tuple>
#include <iostream>
#include <vector>
#include <fstream> 
#include <sstream>
#include <numeric>
#include <cmath>
#include <set>
#include "flowsim_topo_rotor.h"



void SingleLayerRotorSimulator::InitializeLinks(){
    for (uint64_t host_id = 0; host_id < rack_count * hosts_per_rack; host_id++) {
        links.emplace(std::make_pair(std::make_tuple(HOST_TOR, host_id), Link(link_speed, HOST_CHANNELS)));
        links.emplace(std::make_pair(std::make_tuple(TOR_HOST, host_id), Link(link_speed, HOST_CHANNELS)));
    }    
}

std::vector<link_id_t> SingleLayerRotorSimulator::GetLinkIds(const Flow &flow) const{
    std::vector<link_id_t> linkid_list;
    linkid_list.emplace_back(std::make_tuple(HOST_TOR, flow.src_host));
    linkid_list.emplace_back(std::make_tuple(TOR_HOST, flow.dst_host));
    return linkid_list;
}

void SingleLayerRotorSimulator::IncrementLinkFlowCount(const Flow &flow){
    for (const auto &link_id: GetLinkIds(flow)){
        LinkType linktype = std::get<0>(link_id);
        if(linktype == HOST_TOR && flow.src_channel > 0  && flow.src_channel < HOST_CHANNELS){
            links.at(link_id).IncrementFlowCount(flow.src_channel);
            //std::cout << "src channel:" <<  flow.src_channel <<"\n";
            //std::cout << "HOST_TOR" << "\n";
        }
        else if (linktype == TOR_HOST && flow.dst_channel > 0 && flow.dst_channel < HOST_CHANNELS){
            links.at(link_id).IncrementFlowCount(flow.dst_channel);
            //std::cout << "dst channel:" <<  flow.dst_channel <<"\n";
            //std::cout << "TOR_HOST" << "\n";
        }
        else{
            std::cout << "IncrementLinkFlowCount doesn't have a valid chaneel" << "\n";
            //links.at(link_id).IncrementFlowCount(); 
        }
    }   
}

uint64_t SingleLayerRotorSimulator::GetTransmittedBytes(const Flow &flow, double interval) const{
    auto rates = GetRatesPerCycle(flow);
    auto bytes_per_slot = GetBytesPerCycle(flow, rates);
    double rate_down = 0; //GetFlowRateForDownRotors(flow);

    // Get to end of current slot
    double time_in_cycle = fmod(time_now, cycle_time);
    double time_in_slot = fmod(time_in_cycle, total_slot_time);
    int channel_base = (int)ceil(time_in_cycle / total_slot_time);
    uint64_t bytes_transmitted = 0;

    if (time_in_slot < transmit_slot_time) {
        double transmit_slot_remaining_time = transmit_slot_time - time_in_slot;
        if (interval <= transmit_slot_remaining_time) {
            return (uint64_t) (rates[channel_base] * interval);
        } else {
            bytes_transmitted += (uint64_t) (rates[channel_base] * transmit_slot_remaining_time);
            interval -= transmit_slot_remaining_time;
        }

        time_in_slot = transmit_slot_time;
    }

    // Now we are in rotor down time zone
    double whole_slot_remaining_time = total_slot_time - time_in_slot;
    if (interval > whole_slot_remaining_time){
        interval -= whole_slot_remaining_time;
    }  

    // Go through whole cycles
    auto bytes_per_cycle = std::accumulate(bytes_per_slot.begin(), bytes_per_slot.end(), (uint64_t)0);
    auto whole_cycles = (uint64_t)ceil(interval / cycle_time);
    bytes_transmitted += whole_cycles * bytes_per_cycle;

    // Run through remaining time
    double remaining_time = fmod(interval, cycle_time);
    auto num_slots_left = (int)(remaining_time / total_slot_time);
    auto slot_time_left = fmod(remaining_time, total_slot_time);
    bytes_transmitted += std::accumulate(bytes_per_slot.begin(), bytes_per_slot.begin() + num_slots_left, (uint64_t)0);
    //auto channel = (channel_base + whole_slots_left) % channel_count;
    auto slot = num_slots_left + 1;
    if (slot_time_left <= transmit_slot_time) {
        bytes_transmitted += (uint64_t) (slot_time_left * rates[slot]);
    } else {
        bytes_transmitted += (uint64_t) (transmit_slot_time * rates[slot]);
        bytes_transmitted += (uint64_t) (rate_down * (transmit_slot_time - slot_time_left));
    }

    return bytes_transmitted;

}

double SingleLayerRotorSimulator::GetFlowRemainingTime(const Flow &flow) const{    
    auto rates = GetRatesPerCycle(flow);
    auto bytes_per_slot = GetBytesPerCycle(flow, rates);
    uint64_t total_bytes = flow.GetRemainingBytes();

    // Get to end of current slot
    double time_in_cycle = fmod(time_now, cycle_time);
    double time_in_slot = fmod(time_in_cycle, total_slot_time);
    int channel_base = flow.dst_channel; //(int)ceil(time_in_cycle / total_slot_time);

    //double start_partial_slot_time = 0;
    double start_partial_slot_time = 0;
    if (time_in_slot < transmit_slot_time) {
        double transmit_slot_remaining_time = transmit_slot_time - time_in_slot;
        auto partial_bytes = (uint64_t) (rates[channel_base] * transmit_slot_remaining_time);
        if (total_bytes <= partial_bytes) {
            return (double)total_bytes / rates[channel_base];
        } else {
            total_bytes -= partial_bytes;
            start_partial_slot_time += transmit_slot_remaining_time;
        }

        time_in_slot = transmit_slot_time;
    }

    double whole_slot_remaining_time = total_slot_time - time_in_slot;
    start_partial_slot_time += whole_slot_remaining_time;

    // Go through whole cycles 
    uint64_t bytes_per_cycle = std::accumulate(bytes_per_slot.begin(), bytes_per_slot.end(), (uint64_t)0);
    uint64_t whole_cycles = total_bytes / bytes_per_cycle;
    double whole_cycles_time = (double)whole_cycles * cycle_time;

    // Run through remaining slots
    uint64_t remaining_bytes = total_bytes % bytes_per_cycle;
    double remaining_time = 0;
    int slot_index = 0;
    while (remaining_bytes > 0 && slot_index < num_slots) {
        if (remaining_bytes < bytes_per_slot[slot_index]) {
            auto rate = rates[slot_index];
            auto transmit_bytes = (uint64_t)(rate * transmit_slot_time);
            if (remaining_bytes < transmit_bytes) {
                remaining_time += (double)remaining_bytes / rate;
            } else {
                remaining_time += transmit_slot_time;
                remaining_bytes -= transmit_bytes;
                //remaining_time += (double)remaining_bytes / rate_down;
            }
            break;
        }

        remaining_time += total_slot_time;
        remaining_bytes -= bytes_per_slot[slot_index];
        slot_index++;
    }

    if (slot_index >= num_slots){
        std::cout << "slot index error in FlowRemainingTime\n";
        //throw runtime_error("Out of bound on channel index.\n");
    }

    return start_partial_slot_time + whole_cycles_time + remaining_time;      
}

std::tuple<int, int> SingleLayerRotorSimulator::GetFlowChannel(uint64_t src_host, uint64_t dst_host, int slotnum) const{
    int src_channel=-1, dst_channel=-1;
    uint64_t src_index_start = src_host*4;
    uint64_t dst_index_start = dst_host*4;

    for(uint64_t src = src_index_start; src < src_index_start + 4; src++){
        if(dst_host == rotor_matchings[slotnum][src]){
            src_channel = src%4;
        }
    }

    for(uint64_t dst = dst_index_start; dst < dst_index_start + 4; dst++){
        if(src_host == rotor_matchings[slotnum][dst]){
            dst_channel = dst%4;
        }
    }
    return std::make_tuple(src_channel, dst_channel);
}

/*double SingleLayerRotorSimulator::GetFlowRateForDownRotors(const Flow &flow) const{    
    return 0.0;
}*/

std::vector<double> SingleLayerRotorSimulator::GetRatesPerCycle(const Flow &flow) const{
    double time_in_cycle = fmod(time_now, cycle_time);
    //int slotnum = (int)std::floor(time_in_cycle / total_slot_time); 

    //std::cout << "\nnum_slots: " <<  num_slots <<"\n";
    std::vector<double> rate_vec(num_slots);// flow rate at each time slot.
    //std::vector<double> rates(channel_count);// flow rate at each channel.
    std::fill(rate_vec.begin(), rate_vec.end(), (double)0);
    uint64_t dst_host = flow.dst_host;
    uint64_t src_host = flow.src_host;
    //std::cout << "src:" <<  src_host << ", dst:" <<  dst_host <<"\n";
    
    for (int slot = 0; slot < num_slots; slot++){ //which slot in a cycle?
        std::tuple<int, int> channels = GetFlowChannel(src_host,dst_host, slot);
        //std::cout << "src_ch:" <<  std::get<0>(channels) << ", dst_ch:" <<  std::get<1>(channels) <<"\n";
        double flow_rate_onehop = GetFlowRate(flow, channels); // One-hop rate
        rate_vec[slot] = flow_rate_onehop;
        //std::cout << "slots:" <<  slot << ", rate:"<< flow_rate_onehop <<"\n";
        //std::cout << "--------------\n";
        //TODO: two hop needs to be verified!
        #if ROTOR_LB_TWO_HOP
        std::vector<uint64_t> mid_vec = dst_midlist.at(dst_host); //access specified element with bounds checking 
        for (auto mid_host: mid_vec) {
            //two_hop_rendezvous_hosts.at(std::make_tuple(src_host, dst_host))) {
            std::vector<double> link_rates;

            auto src_channels = GetFlowChannel(src_host, mid_host, slot);
            int uplink_channel = std::get<0>(src_channels);
            int mid_channel0 = std::get<1>(src_channels);            

            auto uplink1_id = std::make_tuple(HOST_TOR, src_host);
            auto uplink2_id = std::make_tuple(TOR_HOST, mid_host);
            link_rates.push_back(links.at(uplink1_id).GetRatePerFlow(uplink_channel));
            link_rates.push_back(links.at(uplink2_id).GetRatePerFlow(mid_channel0));

            auto dst_channels    = GetFlowChannel(mid_host, dst_host, slot);
            int mid_channel1     = std::get<0>(dst_channels);
            int downlink_channel = std::get<1>(dst_channels);   

            auto downlink1_id = std::make_tuple(HOST_TOR, mid_host);
            auto downlink2_id = std::make_tuple(TOR_HOST, dst_host);
            link_rates.push_back(links.at(downlink1_id).GetRatePerFlow(mid_channel1));
            link_rates.push_back(links.at(downlink2_id).GetRatePerFlow(downlink_channel));

            auto rate = *min_element(link_rates.begin(), link_rates.end());
            rate_vec[slot] += rate;
        }
        #endif
    }
    return rate_vec;
}

std::vector<uint64_t> SingleLayerRotorSimulator::GetBytesPerCycle(const Flow &flow, std::vector <double> &rates) const{
    std::vector<uint64_t> bytes_per_slot(num_slots); // # of bytes this flow can transmit per time slot.
    for (int slot = 0; slot < num_slots; slot++){
        bytes_per_slot[slot] = (uint64_t) (rates[slot] * transmit_slot_time);
    }
    return bytes_per_slot;
}

void SingleLayerRotorSimulator::InitializeMatchingMatrix(){
    std::ifstream inStream(filename, std::ifstream::in);
    std::string line;

    if(inStream.is_open()){
        std::getline(inStream, line);
        std::stringstream input_parser(line);
        int _useless, num_dnlink, num_uplink, num_hosts, num_slots, num_total_slots; 
        input_parser >> _useless >> num_dnlink >> num_uplink >> num_hosts; // num_hosts in the original format

        input_parser.str(std::string());
        input_parser.clear();// clear any fail or eof flags
        std::getline(inStream, line);
        input_parser << line;
        input_parser >> num_total_slots;
        //a slice = "epsilon" slice + "delta" slice + "r" slice
        num_slots = num_total_slots/3; //we care about a larger time slots;
        std::cout << "downlinks:"<< num_dnlink << ", uplinks:" << num_uplink 
                  << ", hosts:" << num_hosts << ", time slots:" << num_slots << "\n";

        int sizeY=num_slots;
        int sizeX=num_hosts*num_uplink;
        rotor_matchings = new int*[sizeY];
        for(int i = 0; i < sizeY; ++i) {
            rotor_matchings[i] = new int[sizeX];
        }
        
        std::cout << "The matching matrix for "<< num_hosts << " hosts (indexed from 0 to "<< num_hosts << ")\n";
        int line_num = 0;
        while (line_num < num_total_slots){
            std::getline(inStream, line);
            std::istringstream line_parser(line);
            // every three lines are grouped as a "super" slice that we use to construct the conn matrix
            if(line_num%3 == 0){ 
                //std::cout << "index:"<< line_num/3 << "\n";
                for(int columm_index=0; columm_index < num_hosts*num_uplink; columm_index++){
                    line_parser >> rotor_matchings[line_num/3][columm_index];
                }
                for(int columm_index=0; columm_index < num_hosts*num_uplink; columm_index++){
                    std::cout<< rotor_matchings[line_num/3][columm_index]<<" ";
                }
                std::cout<<"\n";
            }
            line_num++;
        }
    }
    else{
        throw std::runtime_error("unable to open input file");
    }

    std::cout<<"end parseing\n";
}

void SingleLayerRotorSimulator::UpdateLinkDemand(){
    double time_in_cycle = fmod(time_now, cycle_time);
    int slotnum = (int)std::floor(time_in_cycle / total_slot_time); //which slot in a cycle?

    fprintf(stderr, "Scanning %zu flows to update demand ...\n", flows.size());
    for (auto &[key, link]: links){
        link.ResetFlowCounts();
    }

    for (const auto &flow: flows) {
        if (!flow.HasStarted(time_now) || flow.IsCompleted()){
            continue;
        }
        IncrementLinkFlowCount(flow);
    }

    for (auto &flow: flows) {
        std::tuple<int, int> channels = GetFlowChannel(flow.src_host, flow.dst_host, slotnum);
        flow.setChannels(channels);
    }

    #if ROTOR_LB_TWO_HOP
    for (const auto &flow: flows) {
        uint64_t src_host = flow.src_host;
        uint64_t dst_host = flow.dst_host;
        uint64_t src_index_start = src_host*4;
        uint64_t dst_index_start = dst_host*4;

        if (!flow.HasStarted(time_now) || flow.IsCompleted()){
            continue;
        }

        if (src_host == dst_host){
            continue;
        }

        std::set<uint64_t> one_hop_dsts; 
        //dst can be reached in two-hop, dst -> vector of mid_hosts
        //std::map<uint64_t, std::vector<uint64_t>> dst_midlist;  

        for(uint64_t src = src_index_start; src < src_index_start + 4; src++){ // 4 uplinks
            one_hop_dsts.insert(rotor_matchings[slotnum][src]);
        }

        for(uint64_t src = src_index_start; src < src_index_start + 4; src++){
            uint64_t mid_host_start =rotor_matchings[slotnum][src]*4;
            for(uint64_t mid= mid_host_start; mid < mid_host_start + 4; mid++){
                //two_hop_dsts.insert(rotor_matchings[slotnum][mid]);
                uint64_t dst_temp =rotor_matchings[slotnum][mid];
                dst_midlist[dst_temp].push_back(mid);  
            }
        }

        for(auto it = one_hop_dsts.begin(); it != one_hop_dsts.end(); ++it) {
            auto search = dst_midlist.find(*it);           
            if (search != dst_midlist.end()) {
                dst_midlist.erase(*it); 
            } 
        }

        for (auto mid_host: dst_midlist[dst_host]) {
            auto uplink1 = std::make_tuple(HOST_TOR, flow.src_host);
            auto uplink2 = std::make_tuple(TOR_HOST, mid_host);
            std::tuple<int, int> channels = GetFlowChannel(flow.src_host, mid_host, slotnum);
            int src_channel  = std::get<0>(channels);
            int mid_channel0 = std::get<1>(channels);  
            links.at(uplink1).IncrementFlowCount(src_channel);
            links.at(uplink2).IncrementFlowCount(mid_channel0);

            auto downlink1 = std::make_tuple(HOST_TOR, mid_host);
            auto downlink2 = std::make_tuple(TOR_HOST, flow.dst_host);
            std::tuple<int, int> channels = GetFlowChannel(mid_host, flow.dst_host, slotnum);
            int mid_channel1 = std::get<0>(channels);
            int dst_channel  = std::get<1>(channels);  
            links.at(downlink1).IncrementFlowCount(mid_channel1);
            links.at(downlink2).IncrementFlowCount(dst_channel);
        }
    }
    #endif

}

// host-0   host-1   host-2   host-3   host-4   host-5   host-6   host-7
// 7 2 0 3, 4 6 3 5, 2 0 4 7, 3 4 1 0, 1 3 2 6, 6 5 7 1, 5 1 6 4, 0 7 5 2
// 7 4 0 3, 4 2 3 5, 2 1 4 7, 3 5 1 0, 1 0 2 6, 6 3 7 1, 5 7 6 4, 0 6 5 2
// 7 4 1 3, 4 2 0 5, 2 1 5 7, 3 5 6 0, 1 0 7 6, 6 3 2 1, 5 7 3 4, 0 6 4 2

