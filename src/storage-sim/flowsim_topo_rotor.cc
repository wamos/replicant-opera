#include <utility>
#include <tuple>
#include <iostream>
#include <vector>
#include <fstream> 
#include <sstream>
#include <numeric>
#include "flowsim_config_macro.h"
#include "flowsim_topo_rotor.h"

void SingleLayerRotorSimulator::InitializeLinks(){
    for (uint64_t host_id = 0; host_id < rack_count * hosts_per_rack; host_id++) {
        //Link link0(link_speed, channel_count, num_slots);
        //link0.InitializeFlowCountMatrix(link_speed, channel_count, num_slots);

        //Link link1(link_speed, channel_count, num_slots);
        //link1.InitializeFlowCountMatrix(link_speed, channel_count, num_slots);

        //links[std::make_tuple(HOST_TOR, host_id)] = std::move(link0);
        //links[std::make_tuple(TOR_HOST, host_id)] = std::move(link1);
        //links.emplace(std::make_pair(std::make_tuple(HOST_TOR, host_id), std::move(link0)));
        //links.emplace(std::make_pair(std::make_tuple(TOR_HOST, host_id), std::move(link1)));

        links.emplace(std::make_pair(std::make_tuple(HOST_TOR, host_id), Link(link_speed, channel_count, num_slots)));
        links.emplace(std::make_pair(std::make_tuple(TOR_HOST, host_id), Link(link_speed, channel_count, num_slots)));
    }    
}

std::vector<link_id_t> SingleLayerRotorSimulator::GetLinkIds(const Flow &flow) const{
    std::vector<link_id_t> linkid_list;
    linkid_list.emplace_back(std::make_tuple(HOST_TOR, flow.src_host));
    linkid_list.emplace_back(std::make_tuple(TOR_HOST, flow.dst_host));
    return linkid_list;
}


void SingleLayerRotorSimulator::IncrementLinkFlowCount(const Flow &flow){
    std::cout << "IncrementLinkFlowCount" << "\n";
    for (int slot = 0; slot < num_slots; slot++){ //which slot in a cycle?  
        std::tuple<int, int> channels = GetFlowChannel(flow.src_host, flow.dst_host, slot);
        int src_channel = std::get<0>(channels);
        int dst_channel = std::get<1>(channels); 
        if(src_channel >= 0 && dst_channel >= 0){
            for (const auto &link_id: GetLinkIds(flow)){
                LinkType linktype = std::get<0>(link_id);
                if(linktype == HOST_TOR && src_channel > 0  && src_channel < HOST_CHANNELS){
                    links.at(link_id).IncrementFlowCount(src_channel, slot);
                    //std::cout << "src channel:" <<  flow.src_channel <<"\n";
                    //std::cout << "HOST_TOR" << "\n";
                }
                else if (linktype == TOR_HOST && dst_channel > 0 && dst_channel < HOST_CHANNELS){
                    links.at(link_id).IncrementFlowCount(dst_channel, slot);
                    //std::cout << "dst channel:" <<  flow.dst_channel <<"\n";
                    //std::cout << "TOR_HOST" << "\n";
                }
                else{
                    std::cout << "IncrementLinkFlowCount doesn't have a valid channel" << "\n";
                    //links.at(link_id).IncrementFlowCount(); 
                }
            }    
        }
    }

}    
// Add flow counts for one-hop flow
/*void SingleLayerRotorSimulator::IncrementLinkFlowCount(const Flow &flow){
    std::cout << "IncrementLinkFlowCount" << "\n";
    std::set<std::tuple<int, int>> channels_set;
    for (int slot = 0; slot < num_slots; slot++){ //which slot in a cycle?  
        std::tuple<int, int> channels = GetFlowChannel(flow.src_host, flow.dst_host, slot);
        if(std::get<0>(channels) >= 0 && std::get<1>(channels) >= 0){
            channels_set.insert(channels);
        }
    }
    if(!channels_set.empty()){
        for (auto iter = channels_set.begin(); iter != channels_set.end(); ++iter){
            std::tuple<int, int> channels = *iter; 
            int src_channel = std::get<0>(channels);
            int dst_channel = std::get<1>(channels); 
            for (const auto &link_id: GetLinkIds(flow)){
                LinkType linktype = std::get<0>(link_id);
                if(linktype == HOST_TOR && src_channel > 0  && src_channel < HOST_CHANNELS){
                    links.at(link_id).IncrementFlowCount(src_channel);
                    //std::cout << "src channel:" <<  flow.src_channel <<"\n";
                    //std::cout << "HOST_TOR" << "\n";
                }
                else if (linktype == TOR_HOST && dst_channel > 0 && dst_channel < HOST_CHANNELS){
                    links.at(link_id).IncrementFlowCount(dst_channel);
                    //std::cout << "dst channel:" <<  flow.dst_channel <<"\n";
                    //std::cout << "TOR_HOST" << "\n";
                }
                else{
                    std::cout << "IncrementLinkFlowCount doesn't have a valid channel" << "\n";
                    //links.at(link_id).IncrementFlowCount(); 
                }
            }
        }
    }
}*/

uint64_t SingleLayerRotorSimulator::GetTransmittedBytes(const Flow &flow, double interval) const{
    std::cout << "GetTransmittedBytes\n";
    auto rates = GetRatesPerCycle(flow);
    auto bytes_per_slot = GetBytesPerCycle(flow, rates);
    double rate_down = 0; //GetFlowRateForDownRotors(flow);

    // Get to end of current slot
    double which_cycle = (int) floor(time_now/cycle_time);
    double time_in_cycle = fmod(time_now, cycle_time);
    double time_in_slot = fmod(time_in_cycle, total_slot_time);
    int slotnum_now = (int)floor(time_in_cycle / total_slot_time);
    uint64_t bytes_transmitted = 0;

    // TODO: we need an exceptional case here, for the rotorlb two-hop
    if (time_in_slot < transmit_slot_time) {
        double transmit_slot_remaining_time = transmit_slot_time - time_in_slot;
        if (interval <= transmit_slot_remaining_time) {
            return (uint64_t) (rates[slotnum_now] * interval);
        } else {
            bytes_transmitted += (uint64_t) (rates[slotnum_now] * transmit_slot_remaining_time);
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
    std::cout << "GetFlowRemainingTime\n";
    auto rates = GetRatesPerCycle(flow);
    auto bytes_per_slot = GetBytesPerCycle(flow, rates);
    uint64_t total_bytes = flow.GetRemainingBytes();
    std::cout<< "total_bytes:" << total_bytes << "\n";

    // Get to end of current slot
    double which_cycle = (int) floor(time_now/cycle_time);
    double time_in_cycle = fmod(time_now, cycle_time);
    double time_in_slot = fmod(time_in_cycle, total_slot_time);
    int slotnum_now = (int)floor(time_in_cycle / total_slot_time);
    std::cout << "slotnum_now:" << slotnum_now << "\n";
    std::cout << "time_in_slot:" << time_in_slot << "\n";

    // TODO: we need an exceptional case here, for the rotorlb two-hop, the first slot in first cycle cannot have a buffered two-hop connection 
    #if ROTOR_LB_TWO_HOP
    double first_slot_rate = 0;
    if( which_cycle ==0 &&  slotnum_now ==0){
        std::cout<<" the first slot in first cycle \n";
        std::tuple<int, int> channels = GetFlowChannel(flow.src_host, flow.dst_host, slotnum_now);
        double flow_rate_onehop = GetFlowRate(flow, channels, slotnum_now); // One-hop rate
        if(rates[slotnum_now] > flow_rate_onehop){
            first_slot_rate = flow_rate_onehop;
        }
        else{
            first_slot_rate = rates[slotnum_now];
        }
    }
    #endif

    double start_partial_slot_time = 0;
    if (time_in_slot < transmit_slot_time) {
        double transmit_slot_remaining_time = transmit_slot_time - time_in_slot;
        uint64_t partial_bytes = 0;

        #if ROTOR_LB_TWO_HOP
        if( which_cycle ==0 &&  slotnum_now ==0){// an edge case for the first slot in first cycle, which cannot have a buffered two-hop connection 
            std::cout<<" the first slot in first cycle \n";
            partial_bytes = (uint64_t) (first_slot_rate *transmit_slot_remaining_time);
        }
        else{
            partial_bytes = (uint64_t) (rates[slotnum_now] * transmit_slot_remaining_time);
        }
        #else
        partial_bytes = (uint64_t) (rates[slotnum_now] * transmit_slot_remaining_time);
        #endif

        if (total_bytes <= partial_bytes) {
            return (double)total_bytes / rates[slotnum_now];
        } else {            
            total_bytes -= partial_bytes;
            std::cout << "total_bytes in a slot:" << partial_bytes << "\n";
            start_partial_slot_time += transmit_slot_remaining_time;
        }

        time_in_slot = transmit_slot_time;
    }
    std::cout << "time_in_slot:" << time_in_slot << "\n";

    //double whole_slot_remaining_time = total_slot_time - time_in_slot;
    //start_partial_slot_time += whole_slot_remaining_time;

    // Go through whole cycles carefully with the first and last cycle
    uint64_t bytes_per_cycle = std::accumulate(bytes_per_slot.begin(), bytes_per_slot.end(), (uint64_t)0);
    std::cout << "bytes_per_cycle:" << bytes_per_cycle << "\n";
    uint64_t whole_cycles = total_bytes / bytes_per_cycle; 
    std::cout << "whole_cycles:" << whole_cycles << "\n";
    double whole_cycles_time = (double) whole_cycles * cycle_time;
    // Minor adjustment
    whole_cycles_time = whole_cycles_time - total_slot_time; //count the slot above twice in this pre-cycle calculation.   

    // Run through remaining slots
    uint64_t remaining_bytes = total_bytes % bytes_per_cycle;
    std::cout << "remaining_bytes:" << remaining_bytes << "\n";
    double remaining_time = 0;
    int slot_index = 0;
    while (remaining_bytes > 0 && slot_index < num_slots) {
        if (remaining_bytes < bytes_per_slot[slot_index]) {
            auto rate = rates[slot_index];
            auto transmit_bytes = (uint64_t)(rate * transmit_slot_time);
            if (remaining_bytes < transmit_bytes) {
                //std::cout << "not enough for a slot\n";
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
    //std::cout<< "slot index:" << slot_index << "\n";

    // edge case handling for the whole cycle time
    if(remaining_time == 0){ 
        // okay we got several whole cycles, but what about the slots with rate 0 at the end of a cycle?
        int cutoff_slots = 0;  
        bool nonzero_start = false, nonzero_to_zero = false;
        for(int i = 1; i < bytes_per_slot.size(); i++){
            if( bytes_per_slot[i-1]==0 && bytes_per_slot[i] > 0){
                nonzero_start = true;
            }

            if( bytes_per_slot[i]==0 && bytes_per_slot[i-1] > 0){
                nonzero_to_zero = true;
            }

            if(nonzero_start && nonzero_to_zero){
                cutoff_slots++;
            }
        }
        std::cout<< "cutoff slots:" << cutoff_slots << "\n";
        //pushing back flow remaining time some slots            
        double time_reduction = (double)cutoff_slots * total_slot_time;
        std::cout << "time_reduction:" << time_reduction << "\n";
        whole_cycles_time = whole_cycles_time - time_reduction;
    }


    if (slot_index >= num_slots){
        std::cout << "slot index error in FlowRemainingTime\n";
        //throw runtime_error("Out of bound on channel index.\n");
    }

    std::cout << "start_partial_slot_time:" << start_partial_slot_time << ", ";
    std::cout << "whole_cycles_time:" << whole_cycles_time << ", ";
    std::cout << "remaining_time:" << remaining_time << "\n";

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

    //std::cout << "GetRatesPerCycle for "<<  num_slots <<" slots\n";
    std::cout << "Start GetRatesPerCycle for "<<  flow.src_host <<"->" << flow.dst_host << "\n";

    std::vector<double> rate_vec(num_slots);// flow rate at each time slot.
    //std::vector<double> rates(channel_count);// flow rate at each channel.
    std::fill(rate_vec.begin(), rate_vec.end(), (double)0);
    uint64_t dst_host = flow.dst_host;
    uint64_t src_host = flow.src_host;
    //std::cout << "src:" <<  src_host << ", dst:" <<  dst_host <<"\n";
    
    for (int slot = 0; slot < num_slots; slot++){ //which slot in a cycle?
        //std::cout << "------------------\n";
        std::cout << "slots:" << slot << "\n";
        std::tuple<int, int> channels = GetFlowChannel(src_host,dst_host, slot);
        //std::cout << "src_ch:" <<  std::get<0>(channels) << ", dst_ch:" <<  std::get<1>(channels) <<"\n";
        double flow_rate_onehop = GetFlowRate(flow, channels, slot); // One-hop rate
        rate_vec[slot] = flow_rate_onehop;
        //std::cout<< std::fixed << "slots:" <<  slot << ", one-hop rate:"<< flow_rate_onehop << "\n";

        #if BUFFERLESS_TWO_HOP
        auto slot_mapset = dst_midlist.at(slot);
        #endif

        #if ROTOR_LB_TWO_HOP
        auto slot_mapset = rotorlb_midlist.at(slot);
        #endif

        auto found_dst = slot_mapset.find(dst_host);
        if (found_dst != slot_mapset.end()) {
            for (const auto mid_host: slot_mapset.at(dst_host)) {
                std::vector<double> link_rates;  
                //std::cout <<"two-hop path:"<< src_host << "->" << mid_host << "->" << dst_host << "\n";
                auto src_channels = GetFlowChannel(src_host, mid_host, slot);
                int uplink_channel = std::get<0>(src_channels);
                int mid_channel0 = std::get<1>(src_channels);            

                auto uplink1_id = std::make_tuple(HOST_TOR, src_host);
                auto uplink2_id = std::make_tuple(TOR_HOST, mid_host);
                link_rates.push_back(links.at(uplink1_id).GetRatePerTwohopFlow(uplink_channel, slot));
                link_rates.push_back(links.at(uplink2_id).GetRatePerTwohopFlow(mid_channel0, slot));

                auto dst_channels    = GetFlowChannel(mid_host, dst_host, slot);
                int mid_channel1     = std::get<0>(dst_channels);
                int downlink_channel = std::get<1>(dst_channels);   

                auto downlink1_id = std::make_tuple(HOST_TOR, mid_host);
                auto downlink2_id = std::make_tuple(TOR_HOST, dst_host);
                link_rates.push_back(links.at(downlink1_id).GetRatePerTwohopFlow(mid_channel1, slot));
                link_rates.push_back(links.at(downlink2_id).GetRatePerTwohopFlow(downlink_channel, slot));

                /*std::cout << "link rates:\n";
                for(auto r: link_rates){
                    std::cout << std::fixed << r << ",";
                }
                std::cout << "\n";*/
                
                auto flow_rate_twohop = *min_element(link_rates.begin(), link_rates.end());
                std::cout << std::fixed << "twp-hop rate:"<< flow_rate_twohop <<"\n";

                #if ROTOR_LB_TWO_HOP
                //int next_slot = (slot+1)%num_slots;
                rate_vec[slot] += flow_rate_twohop;
                #endif

                #if BUFFERLESS_TWO_HOP
                rate_vec[slot] += flow_rate_twohop;
                #endif
            } 
        }
        else{
           std::cout << "no two-hop path for dst " << dst_host << ", check one-hop path\n"; 
        }
        std::cout << std::fixed << "rate_vec:" << rate_vec[slot] << "\n";
    }
    std::cout << "End GetRatesPerCycle for "<<  flow.src_host <<"->" << flow.dst_host << "\n";
    return rate_vec;
}

std::vector<uint64_t> SingleLayerRotorSimulator::GetBytesPerCycle(const Flow &flow, std::vector <double> &rates) const{
    std::cout << "GetBytesPerCycle\n";
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

#if TWO_HOP_PATH
// This is just like a tow-hop version of IncrementLinkFlowCount()
// we don't really care that if the flow has a connectivity from src to dst
// If they are not connected, the rate is just zero. But the link still contains the flow
bool SingleLayerRotorSimulator::UpdateTwoHopLinkFlowCount(const Flow &flow){
    uint64_t src_host = flow.src_host;
    uint64_t dst_host = flow.dst_host;
    //Path sets construction, shared by bufferless and store-and-forward two-hop
    for (int slot = 0; slot < num_slots; slot++){
        std::set<uint64_t> one_hop_dsts; 
        std::map<uint64_t, std::set<uint64_t>> slot_mapset; //dst can be reached in two-hop, dst -> vector of mid_hosts    
        uint64_t src_index_start = src_host*channel_count;
        uint64_t dst_index_start = dst_host*channel_count;

        if (!flow.HasStarted(time_now) || flow.IsCompleted()){
            return false;
        }

        if (src_host == dst_host){
            return false;
        }
        
        for(uint64_t src = src_index_start; src < src_index_start + channel_count; src++){ // 4 uplinks
            one_hop_dsts.insert(rotor_matchings[slot][src]);
        }

        //#if BUFFERLESS_TWO_HOP
        for(uint64_t src = src_index_start; src < src_index_start + channel_count; src++){
            uint64_t mid_host_start =rotor_matchings[slot][src]*channel_count;
            for(uint64_t mid = mid_host_start; mid < mid_host_start + channel_count; mid++){
                //two_hop_dsts.insert(rotor_matchings[slotnum][mid]);
                uint64_t dst_temp = rotor_matchings[slot][mid];
                if(dst_temp == dst_host){
                    slot_mapset[dst_temp].insert(mid/channel_count);  
                    //two_hop_set[dst_temp].insert(mid/channel_count);
                }
            }
        }
        for(auto it = one_hop_dsts.begin(); it != one_hop_dsts.end(); ++it) {
            auto foundit = slot_mapset.find(*it);           
            if (foundit != slot_mapset.end()) {
                slot_mapset.erase(*it); 
            } 
        }
        //dst_midlist.push_back(std::move(slot_mapset));
        auto& mapset =dst_midlist[slot];
        mapset[dst_host] = slot_mapset[dst_host];
        //dst_midlist[slot] = std::move(slot_mapset);
        //#endif
    }

    /*std::cout << "--------------per-slot path set--------------"<<"\n";
    for (int slot = 0; slot < num_slots; slot++){
        std::cout << "slot:"<< slot <<"\n"; 
        auto slot_mapset = dst_midlist.at(slot);
        for(auto map_iter = slot_mapset.begin(); map_iter != slot_mapset.end(); ++map_iter){
             std::cout << "dst:"<< map_iter->first << "<-";
             auto setit = map_iter->second;
             for(auto set_iter = setit.begin(); set_iter != setit.end(); ++set_iter){
                 std::cout << *set_iter <<",";
             }
             std::cout <<"\n";    
        }
        std::cout << "--------------per-slot path set--------------"<<"\n";    
    }*/

    //std::set<uint64_t> two_hop_set; //dst -> all distinct mid_host over all time slot
    //std::set<int> src_channels_set;
    //std::set<std::tuple<int, int>> channels_mid_dst;

    #if ROTOR_LB_TWO_HOP
    for (int slot = 0; slot < num_slots; slot++){
        std::set<uint64_t> rotorlb_slotset;
        auto slot_now  = dst_midlist.at(slot%num_slots);
        auto slot_next = dst_midlist.at((slot+1)%num_slots);
        auto found_dst_now = slot_now.find(dst_host);
        auto found_dst_next = slot_next.find(dst_host);
        if(found_dst_now != slot_now.end() && found_dst_next != slot_next.end()){
            //std::cout << "slot:" << slot << "\n"; 
            auto midlist_now = slot_now.at(dst_host);
            auto midlist_next = slot_next.at(dst_host);
            //find overlapped elements in vec mid_list_now and mid_list_next
            for(auto mid_host:midlist_now){
                auto found_rotorlb = midlist_next.find(mid_host);                
                // 1. ensure we found a mid_host existed in two consecutive slots: slot_now and slot_next
                // ? 2. ensure a distinct two-hop path for this topo
                if(found_rotorlb != midlist_next.end()){ //&& two_hop_set.find(mid_host) == two_hop_set.end()){
                    std::cout << "slot:" << slot << "\n"; 
                    std::cout <<"rotorlb path:"<< src_host << "->" << mid_host << "->" << dst_host << "\n";
                    auto uplink1 = std::make_tuple(HOST_TOR, flow.src_host);
                    auto uplink2 = std::make_tuple(TOR_HOST, mid_host);
                    std::tuple<int, int> channels = GetFlowChannel(flow.src_host, mid_host, (slot+1)%num_slots );
                    int src_channel  = std::get<0>(channels);
                    int mid_channel0 = std::get<1>(channels);
                    /*auto existed = src_channels_set.find(src_channel);
                    if(existed == src_channels_set.end()){ 
                        links.at(uplink1).IncrementTwohopFlowCount(src_channel, slot);
                        src_channels_set.insert(src_channel);
                    }
                    else{
                        std::cout<< "overlap channel on " << src_channel << "\n";
                    }*/
                    links.at(uplink1).IncrementTwohopFlowCount(src_channel, (slot+1)%num_slots);
                    links.at(uplink2).IncrementTwohopFlowCount(mid_channel0, (slot+1)%num_slots);

                    auto downlink1 = std::make_tuple(HOST_TOR, mid_host);
                    auto downlink2 = std::make_tuple(TOR_HOST, flow.dst_host);
                    channels = GetFlowChannel(mid_host, flow.dst_host, slot);
                    int mid_channel1 = std::get<0>(channels);
                    int dst_channel  = std::get<1>(channels);  
                    links.at(downlink1).IncrementTwohopFlowCount(mid_channel1, (slot+1)%num_slots);
                    links.at(downlink2).IncrementTwohopFlowCount(dst_channel, (slot+1)%num_slots);

                    //two_hop_set.insert(mid_host);
                    //std::cout << "-> midhost:" << mid_host << "\n";
                    rotorlb_slotset.insert(mid_host); 
                }
                /*else if(found_rotorlb != midlist_next.end()){
                     std::cout << "-> midhost:" << mid_host << "\n";
                    rotorlb_slot_mapset[dst_host].insert(mid_host);
                }*/
                //else you screwed up
            }
        }
        //else you screwed up
        auto& map_set = rotorlb_midlist[(slot+1)%num_slots];
        map_set[dst_host] = rotorlb_slotset; 
        //std::move(rotorlb_slot_mapset);
        //rotorlb_midlist[(slot+1)%num_slots]= std::move(rotorlb_slot_mapset);
    }

    /*for (int slot = 0; slot < num_slots; slot++){
        auto map = rotorlb_midlist[slot];
        std::set<uint64_t> dst_set = map[dst_host];
        for(auto iter = dst_set.begin(); iter != dst_set.end(); ++iter){
            std::cout << "slot:" << slot << " dst_set:" << *iter <<"\n";
        }
    }*/
    #endif

    #if BUFFERLESS_TWO_HOP
    for (int slot = 0; slot < num_slots; slot++){
        auto slot_mapset = dst_midlist.at(slot);
        auto found_dst = slot_mapset.find(dst_host);
        if(found_dst != slot_mapset.end()){
            for(auto mid_host:slot_mapset.at(dst_host)){
                // ensure an distinct two-hop path for this topo
                if(two_hop_set.find(mid_host) == two_hop_set.end()){
                    std::cout <<"two-hop path:"<< src_host << "->" << mid_host << "->" << dst_host << "\n";
                    auto uplink1 = std::make_tuple(HOST_TOR, flow.src_host);
                    auto uplink2 = std::make_tuple(TOR_HOST, mid_host);
                    std::tuple<int, int> channels = GetFlowChannel(flow.src_host, mid_host, slot);
                    int src_channel  = std::get<0>(channels);
                    int mid_channel0 = std::get<1>(channels);
                    auto existed = src_channels_set.find(src_channel);
                    if(existed == src_channels_set.end()){ 
                        links.at(uplink1).IncrementTwohopFlowCount(src_channel, slot);
                        src_channels_set.insert(src_channel);
                    }
                    else{
                        std::cout<< "overlap channel on " << src_channel << "\n";
                    }
                    links.at(uplink2).IncrementTwohopFlowCount(mid_channel0, slot);

                    auto downlink1 = std::make_tuple(HOST_TOR, mid_host);
                    auto downlink2 = std::make_tuple(TOR_HOST, flow.dst_host);
                    channels = GetFlowChannel(mid_host, flow.dst_host, slot);
                    int mid_channel1 = std::get<0>(channels);
                    int dst_channel  = std::get<1>(channels);  
                    links.at(downlink1).IncrementTwohopFlowCount(mid_channel1, slot);
                    links.at(downlink2).IncrementTwohopFlowCount(dst_channel, slot);
                    two_hop_set.insert(mid_host);               
                }
                //else{std::cout << "threre is an existing two-hop path\n"};
            }
        }
        
        //else{std::cout << "no two-hop path for dst " << dst_host << ", check one-hop path\n";}
    }// for slots
    #endif

    return true;
}
#endif

void SingleLayerRotorSimulator::UpdateLinkDemand(){
    double time_in_cycle = fmod(time_now, cycle_time);
    int slotnum = (int)std::floor(time_in_cycle / total_slot_time); //which slot in a cycle?

    fprintf(stderr, "Scanning %zu flows to update demand ...\n", flows.size());
    for (auto &[key, link]: links){
        link.ResetFlowCounts();
    }

    /*for (auto &flow: flows) {
        std::tuple<int, int> channels = GetFlowChannel(flow.src_host, flow.dst_host, slotnum);
        flow.setChannels(channels);
    }*/

    for (const auto &flow: flows) {
        if (!flow.HasStarted(time_now) || flow.IsCompleted()){
            continue;
        }
        IncrementLinkFlowCount(flow);
        std::cout << "Updating single-hop flow:" << flow.src_host << "->" << flow.dst_host << "\n";
        #if TWO_HOP_PATH
        std::cout << "Updating two-hop flow:" << flow.src_host << "->" << flow.dst_host << "\n";
        bool updated = UpdateTwoHopLinkFlowCount(flow);
        #endif
    }

    /*#if TWO_HOP_PATH
    for (const auto &flow: flows) {
        if (!flow.HasStarted(time_now) || flow.IsCompleted()){
            continue;
        }
        std::cout << "Updating two-hop flow:" << flow.src_host << "->" << flow.dst_host << "\n";
        bool updated = UpdateTwoHopLinkFlowCount(flow);
        if(updated)
            std::cout << "Updating two-hop flow:" << flow.src_host << "->" << flow.dst_host << "\n";
    }
    #endif*/

}

// host-0   host-1   host-2   host-3   host-4   host-5   host-6   host-7
// 7 2 0 3, 4 6 3 5, 2 0 4 7, 3 4 1 0, 1 3 2 6, 6 5 7 1, 5 1 6 4, 0 7 5 2
// 7 4 0 3, 4 2 3 5, 2 1 4 7, 3 5 1 0, 1 0 2 6, 6 3 7 1, 5 7 6 4, 0 6 5 2
// 7 4 1 3, 4 2 0 5, 2 1 5 7, 3 5 6 0, 1 0 7 6, 6 3 2 1, 5 7 3 4, 0 6 4 2
// 7 4 1 6, 4 2 0 7, 2 1 5 3, 3 5 6 2, 1 0 7 5, 6 3 2 4, 5 7 3 0, 0 6 4 1
// 5 4 1 6, 1 2 0 7, 6 1 5 3, 7 5 6 2, 4 0 7 5, 0 3 2 4, 2 7 3 0, 3 6 4 1
// 5 2 1 6, 1 6 0 7, 6 0 5 3, 7 4 6 2, 4 3 7 5, 0 5 2 4, 2 1 3 0, 3 7 4 1
// 5 2 0 6, 1 6 3 7, 6 0 4 3, 7 4 1 2, 4 3 2 5, 0 5 7 4, 2 1 6 0, 3 7 5 1
// 5 2 0 3, 1 6 3 5, 6 0 4 7, 7 4 1 0, 4 3 2 6, 0 5 7 1, 2 1 6 4, 3 7 5 2

