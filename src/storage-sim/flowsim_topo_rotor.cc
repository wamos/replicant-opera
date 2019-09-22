#include <utility>
#include <tuple>
#include <iostream>
#include <vector>
#include <fstream> 
#include <sstream>
#include <numeric>

#include "flowsim_config_macro.h"
#include "flowsim_topo_rotor.h"
#include "util.h"


//Fixed
uint64_t SingleLayerRotorSimulator::GetInterTorLinkId(uint64_t src_rack, uint64_t dst_rack) const{
    return (src_rack << 32) | (dst_rack & 0xffffffff);
}

std::vector<int> SingleLayerRotorSimulator::GetConnectedTORs(uint64_t dst_rack, int slice_num) const{
    int slice = slice_num % num_slots;
    uint64_t dst_index_start = dst_rack*channel_count;    
    std::vector<int> dst_list;
    for(uint64_t dst = dst_index_start; dst < dst_index_start + channel_count; dst++){
        dst_list.push_back(rotor_matchings[slice][dst]); 
    }
    //std::cout << +dst_rack << " is connected to tor:" << dst_list[0] << "," << dst_list[1] << "," << dst_list[2] << "," << dst_list[3] << "\n";
    return dst_list;
}

//TODO: host link rare is now 400 Gbps
void SingleLayerRotorSimulator::InitializeLinks(){
    double tor_link_speed = link_speed * (double)hosts_per_rack;

    for (uint64_t host_id = 0; host_id < rack_count * hosts_per_rack; host_id++) {
        links.emplace(std::make_pair(std::make_tuple(HOST_TOR, host_id), Link(link_speed, channel_count, num_slots)));
        links.emplace(std::make_pair(std::make_tuple(TOR_HOST, host_id), Link(link_speed, channel_count, num_slots)));
    }

    for (size_t src_rack = 0; src_rack < rack_count; src_rack++){
        for (size_t dst_rack = 0; dst_rack < rack_count; dst_rack++) {
            if (src_rack == dst_rack)
                continue;
            uint64_t tor_tor_id = GetInterTorLinkId(src_rack, dst_rack);
            links.emplace(make_pair(std::make_tuple(TOR_ROTOR, tor_tor_id), Link(link_speed, channel_count)));
            links.emplace(make_pair(std::make_tuple(ROTOR_TOR, tor_tor_id), Link(link_speed, channel_count)));
        }
    }

}


std::vector<link_id_t> SingleLayerRotorSimulator::GetInterTORLinkIds(uint64_t src_rack , uint64_t dst_rack) const {
    std::vector<link_id_t> linkid_list;
    if (src_rack != dst_rack) {
        uint64_t tor_tor_id = GetInterTorLinkId(src_rack, dst_rack);
        linkid_list.emplace_back(std::make_tuple(TOR_ROTOR, tor_tor_id));
        linkid_list.emplace_back(std::make_tuple(ROTOR_TOR, tor_tor_id));
    }
    return linkid_list;
}

//Fixed
std::vector<link_id_t> SingleLayerRotorSimulator::GetLinkIds(const Flow &flow) const{
    std::vector<link_id_t> linkid_list;
    linkid_list.emplace_back(std::make_tuple(HOST_TOR, flow.src_host));

    const uint64_t mask = 0xffffffff;
    uint64_t src_rack = cluster.GetRackId(flow.src_host); //& mask;
    uint64_t dst_rack = cluster.GetRackId(flow.dst_host); //& mask;
    if (src_rack != dst_rack) {
        uint64_t tor_tor_id = GetInterTorLinkId(src_rack, dst_rack);
        linkid_list.emplace_back(std::make_tuple(TOR_ROTOR, tor_tor_id));
        linkid_list.emplace_back(std::make_tuple(ROTOR_TOR, tor_tor_id));
    }

    linkid_list.emplace_back(std::make_tuple(TOR_HOST, flow.dst_host));
    return linkid_list;
}

//TODO: This needs to be added a section to support ROTOR_LB_TWO_HOP
uint64_t SingleLayerRotorSimulator::GetTransmittedBytes(const Flow &flow, double interval) const{
    std::cout << "---------------------\nGetTransmittedBytes\n";
    std::cout << flow.src_host << "->" << flow.dst_host << "\n";
    uint64_t total_bytes = flow.GetRemainingBytes();
    std::cout<< "total_bytes:" << total_bytes << "\n";

    auto rates = GetRatesPerCycle(flow);
    auto bytes_per_slot = GetBytesPerCycle(flow, rates);
    double rate_down = 0; //GetFlowRateForDownRotors(flow);

    // Get to end of current slot
    double which_cycle = round(time_now/cycle_time);
    double time_in_cycle = fmod(time_now, cycle_time);
    double time_in_slot = fmod(time_in_cycle, total_slot_time);
    int slotnum_now = floor(time_in_cycle / total_slot_time);


    uint64_t partial_slot_bytes = 0; 
    //if (time_in_slot < transmit_slot_time && time_in_slot > 0) { 
    if (time_in_slot < transmit_slot_time && time_in_slot > 0) {
        double transmit_slot_remaining_time = transmit_slot_time - time_in_slot;
        if (interval <= transmit_slot_remaining_time) {
            return (uint64_t) (rates[slotnum_now] * interval);
        }
        else{ //interval > transmit_slot_remaining_time
            partial_slot_bytes = (uint64_t) (rates[slotnum_now] * transmit_slot_remaining_time);
            std::cout << "partial_slot_bytes" << partial_slot_bytes << "\n";
            interval = interval - transmit_slot_remaining_time; 
        }
        slotnum_now++;
    }

    // several slots time but less than a cycle time
    uint64_t starting_slots_bytes = 0;
    if(interval >= total_slot_time && interval < cycle_time){
        int slot_counter = 0;
        while (interval > 0 && slot_counter < num_slots) {
            starting_slots_bytes = starting_slots_bytes + rates[slotnum_now]*transmit_slot_time;
            interval = interval - total_slot_time;
            slotnum_now++;
            slot_counter++;
        }
        std::cout << "starting_slots_bytes" << starting_slots_bytes << "\n";
    }

    // Now we are in rotor down time zone
    /*double whole_slot_remaining_time = total_slot_time - time_in_slot;
    if (interval > whole_slot_remaining_time){
        interval -= whole_slot_remaining_time;
    }*/
    uint64_t first_cycle_bytes = 0; 
    if(interval >= cycle_time){
        std::vector<double> rate_firstcycle = GetRatesForFirstCycle(flow);
        auto first_cycle_vec = GetBytesPerCycle(flow, rate_firstcycle);  
        first_cycle_bytes = std::accumulate(first_cycle_vec.begin(), first_cycle_vec.end(), (uint64_t)0);
        interval -= cycle_time;
        std::cout << "first_cycle_bytes" << first_cycle_bytes << "\n";
    }

    // Go through whole cycles
    uint64_t whole_cycle_bytes = 0;
    if(interval >= cycle_time){
        auto bytes_per_cycle = std::accumulate(bytes_per_slot.begin(), bytes_per_slot.end(), (uint64_t)0);
        std::cout << "raw whole_cycles:" << interval / cycle_time << "\n";
        double whole_cycles = floor(interval / cycle_time);    
        std::cout << "whole_cycles:" << whole_cycles << "\n";
        whole_cycle_bytes += whole_cycles * bytes_per_cycle;
        std::cout << "whole_cycle_bytes" << whole_cycle_bytes << "\n";
        interval = interval - whole_cycles*cycle_time;
        //slotnum_now++;
    }

    // Run through remaining time slots
    double remaining_time = interval; //fmod(interval, cycle_time);//std::cout << "interval:" << interval << "\n";
    std::cout << "remaining_time:" << remaining_time << "\n";
    uint64_t remaining_slots_bytes = 0;
    if(remaining_time > 0){
        auto num_slots_left = round(remaining_time / total_slot_time);
        std::cout << "num_slots_left:" << num_slots_left << "\n";
        for(int i = 0; i < num_slots_left; i++){
            remaining_slots_bytes = remaining_slots_bytes + rates[slotnum_now]*transmit_slot_time;
            slotnum_now = (slotnum_now + 1)%num_slots;
        }
        //remaining_slots_bytes = std::accumulate(bytes_per_slot.begin(), bytes_per_slot.begin() + num_slots_left, (uint64_t)0);
        std::cout << "bytes_in_slots:" << remaining_slots_bytes << "\n";
    }



    /*auto slot_time_left = fmod(remaining_time, total_slot_time);
    std::cout << "slot_time_left:" << slot_time_left << "\n";*/

    //bytes_transmitted += std::accumulate(bytes_per_slot.begin(), bytes_per_slot.begin() + num_slots_left, (uint64_t)0);
    //auto channel = (channel_base + whole_slots_left) % channel_count;
    /*uint64_t bytes_in_partial_slots = 0;
    auto slot = num_slots_left + 1;
    if (slot_time_left <= transmit_slot_time) {
        bytes_transmitted += (uint64_t) (slot_time_left * rates[slot]);
        bytes_in_partial_slots    += (uint64_t) (slot_time_left * rates[slot]);
    } else {
        bytes_transmitted += (uint64_t) (transmit_slot_time * rates[slot]);
        bytes_in_partial_slots += (uint64_t) (rate_down * (transmit_slot_time - slot_time_left));
    }
    std::cout << "bytes_in_partial_slots:" << bytes_in_partial_slots << "\n";*/

    uint64_t bytes_transmitted = partial_slot_bytes + starting_slots_bytes + first_cycle_bytes + whole_cycle_bytes + remaining_slots_bytes;
    // sometimes remaining_slots_bytes can provide more than total_bytes - (partial_slot_bytes + starting_slots_bytes + first_cycle_bytes + whole_cycle_bytes)
    // to avoid weird situation, we add a minor fix here
    if(bytes_transmitted > total_bytes){
        bytes_transmitted = total_bytes;
    }
    std::cout << "bytes_transmitted:" << bytes_transmitted << "\n";

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

    double start_partial_slot_time = 0;
    uint64_t partial_bytes = 0;
    // This is for the case that the flow_remaining_time starts within a partial slot
    if (time_in_slot < transmit_slot_time && time_in_slot > 0) { 
        double transmit_slot_remaining_time = transmit_slot_time - time_in_slot;
        partial_bytes = (uint64_t) (rates[slotnum_now] * transmit_slot_remaining_time);

        if (total_bytes <= partial_bytes) {
            return (double)total_bytes / rates[slotnum_now];
        } else { // total_bytes > partial_bytes          
            total_bytes -= partial_bytes;
            start_partial_slot_time += transmit_slot_remaining_time;
            std::cout << "total_bytes in a slot:" << partial_bytes << "\n";
            time_in_slot = start_partial_slot_time;
        }
        slotnum_now++;
    }
    //std::cout << "start_partial_slot_time:" << start_partial_slot_time << "\n";

    std::vector<double> rate_firstcycle = GetRatesForFirstCycle(flow);
    auto first_cycle_bytes = GetBytesPerCycle(flow, rate_firstcycle);
    uint64_t first_cycle_throughput = std::accumulate(first_cycle_bytes.begin(), first_cycle_bytes.end(), (uint64_t)0);
    std::cout<< "total_bytes in the first cycle:" << first_cycle_throughput << "\n";
    double first_cycle_time = 0;
    if(total_bytes > first_cycle_throughput && first_cycle_throughput > 0){
        total_bytes = total_bytes - first_cycle_throughput;
        first_cycle_time = cycle_time;
    }

    // Go through whole cycles carefully with the first and last cycle
    uint64_t bytes_per_cycle = std::accumulate(bytes_per_slot.begin(), bytes_per_slot.end(), (uint64_t)0);
    std::cout << "bytes_per_cycle:" << bytes_per_cycle << "\n";
    double whole_cycles_time = 0;
    if( total_bytes > bytes_per_cycle && bytes_per_cycle > 0){
        uint64_t whole_cycles = total_bytes / bytes_per_cycle; 
        std::cout << "whole_cycles:" << whole_cycles << "\n";
        whole_cycles_time = (double) whole_cycles * cycle_time;
        total_bytes = total_bytes - whole_cycles*bytes_per_cycle;
        // we start from previous slotnum_now, and staying at the same slot after cycles
        // for the remaining slots, we need to go to the next slot as a start point 
        // slotnum_now++; 
    }

    // Run through remaining slots
    uint64_t remaining_bytes = total_bytes; //% bytes_per_cycle;
    std::cout << "remaining_bytes:" << remaining_bytes << "\n";
    double remaining_time = 0;
    int slot_counter = 0;
    while (remaining_bytes > 0 && slot_counter < num_slots) {
        if (remaining_bytes < bytes_per_slot[slotnum_now]) {
            auto rate = rates[slotnum_now];
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
        else{
            remaining_time += total_slot_time;
            remaining_bytes -= bytes_per_slot[slotnum_now];
            slotnum_now = (slotnum_now + 1)%num_slots;
            slot_counter++;
        }
    }
    
    // edge case handling for the whole cycle time
    if(remaining_time == 0 && whole_cycles_time > 0){
        // okay we got several whole cycles, but what about the slots with rate 0 at the end of a cycle?
        auto rate_vec = deliver_rate_matrix.at(flow.flow_id);
        //in reverse order
        //for(auto iter = rate_vec.rbegin(); iter != rate_vec.rend(); iter++)
        int rev_index = 0;
        int last_index = rate_vec.size() - 1;
        for(rev_index = 0; rev_index < rate_vec.size(); rev_index++){
            if(rate_vec[last_index - rev_index] != 0.0){
                break;
            }
            else{
                continue;
            }
        }
        std::cout<< "reduction:" << rev_index << "\n";
        whole_cycles_time = whole_cycles_time - rev_index*total_slot_time;
    }

    std::cout << "start_partial_slot_time:" << start_partial_slot_time << ", ";
    std::cout << "first_cycle_time:" << first_cycle_time << ", ";
    std::cout << "whole_cycles_time:" << whole_cycles_time << ", ";
    std::cout << "remaining_time:" << remaining_time << "\n";

    return start_partial_slot_time + first_cycle_time + whole_cycles_time + remaining_time;      
}

//Fixed
std::tuple<int, int> SingleLayerRotorSimulator::GetFlowChannel(uint64_t src_rack, uint64_t dst_rack, int slice) const{
    int src_channel=-1, dst_channel=-1;
    // channel_count = 4 so the following 4 should be replaced to channel_count
    uint64_t src_index_start = src_rack*4;
    uint64_t dst_index_start = dst_rack*4;

    for(uint64_t src = src_index_start; src < src_index_start + 4; src++){
        if(dst_rack == rotor_matchings[slice][src]){
            src_channel = src%4;
        }
    }

    for(uint64_t dst = dst_index_start; dst < dst_index_start + 4; dst++){
        if(src_rack == rotor_matchings[slice][dst]){
            dst_channel = dst%4;
        }
    }
    return std::make_tuple(src_channel, dst_channel);
}

int SingleLayerRotorSimulator::GetFlowTORChannel(uint64_t src_rack, uint64_t dst_rack, int slice) const{
    int src_channel=-1;
    uint64_t src_index_start = src_rack*4;
    for(uint64_t src = src_index_start; src < src_index_start + 4; src++){
        if(dst_rack == rotor_matchings[slice][src]){
            src_channel = src%4;
        }
    }
    return src_channel;
}

double SingleLayerRotorSimulator::GetFlowRateForDownRotors(const Flow &flow) const{    
    /*uint64_t src_rack = cluster.GetRackId(flow.src_host);
    uint64_t dst_rack = cluster.GetRackId(flow.dst_host);
    if (src_rack == dst_rack) {
        return GetFlowRate(flow);
    } else {
        return 0;
    }*/
}

std::vector<double> SingleLayerRotorSimulator::GetRatesForFirstCycle(const Flow &flow) const{
    return first_cycle_rate.at(flow.flow_id);
}

std::vector<double> SingleLayerRotorSimulator::GetRatesPerCycle(const Flow &flow) const{
    return deliver_rate_matrix.at(flow.flow_id);
}

std::vector<uint64_t> SingleLayerRotorSimulator::GetBytesPerCycle(const Flow &flow, std::vector <double> &rates) const{
    //std::cout << "GetBytesPerCycle\n";
    std::vector<uint64_t> bytes_per_slot(rates.size()); // # of bytes this flow can transmit per time slot.
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


void SingleLayerRotorSimulator::InitializeFlowMatrices(){
    for(auto map_iter = cluster.tors.begin(); map_iter != cluster.tors.end(); ++map_iter){ // sweep mid nodes
        uint64_t tor_id = map_iter->first;
        auto& tor = cluster.tors[tor_id];
        tor.InitFlowMatrices(cluster.tors.size());
    }
}

void SingleLayerRotorSimulator::InitializePerTORMatrices(){
    for(auto map_iter = cluster.tors.begin(); map_iter != cluster.tors.end(); ++map_iter){ // sweep mid nodes
        uint64_t tor_id = map_iter->first;
        auto& tor = cluster.tors[tor_id];
        tor.InitFlowMatrices(cluster.tors.size());
    }
}

// TODO: check correntness
double SingleLayerRotorSimulator::deliverTORFlows(TORSwitch& send_tor, std::vector<uint64_t> src_hosts, std::vector<uint64_t> dst_hosts, double tor_capacity, int channel, bool isLocalFlow, int slice){
    //int slice = slice_num % num_slots;
    std::vector<Flow> flows;
    std::vector<double> flowsize_list;

    for(auto src_id: src_hosts){
        for(auto dst_id: dst_hosts){
            auto key_tuple = std::make_tuple(src_id, dst_id);
            auto flow_found = flow_map.find(key_tuple);
            if (flow_found != flow_map.end()) {
                auto flow_vec = flow_found->second;
                for (auto &flow: flow_vec) {
                    flows.push_back(flow);
                    double remaining_bytes = 0;
                    if(isLocalFlow == true){
                        remaining_bytes = send_tor.getLocalFlowByID(flow.flow_id);
                    }
                    else{ // load buffer_flows, send_tor is a mid_tor
                        remaining_bytes = send_tor.getBufferFlowByID(flow.flow_id);
                    }
                    flowsize_list.push_back(remaining_bytes); // remaining flow size on this host
                }
            }
        }
    }

    auto flowupdates = util::fairshare1d(flowsize_list, tor_capacity, true);
    //TODO: 
    // We can have a per-ToR dedicated local-only flow list
    // 1. local-only flows share some capacity of links that connects host to ToR at src-tor 
    // 2. local-only flows share some capacity of links that connects host to ToR at dst-tor
    // 3. update these local-only flows at deliver_rate_matrix with flow list

    double total_flowsize = 0;
    for(int index = 0 ; index < flows.size(); index++){
        auto flow = flows[index];
        double updated_size = flowupdates[index];
        total_flowsize += updated_size;
        if(isLocalFlow == true){ // local
            send_tor.decrementLocalFlow(flow.flow_id, updated_size);
        } 
        else{ //send_tor is a mid_tor that buffers flows
            //TODO needs to check buffer_flow[flow_id] exists!
            send_tor.decrementBufferFlow(flow.flow_id, updated_size);
        }
        //update the global rate matrix for the flow
        auto& rate_vec = deliver_rate_matrix.at(flow.flow_id); 
        rate_vec[slice] += updated_size/transmit_slot_time;
        // check if update_size no greater than the bottlebeck on links of the flow
        double valid_update = checkLinkCapacity(flow, channel, updated_size, slice);
        //update the capacity for used links with valid update size
        updateLinkCapacity(flow, channel, valid_update, slice);
    }
    
    return total_flowsize;
}


void SingleLayerRotorSimulator::RotorBufferedTransfer(TORSwitch& send_tor, TORSwitch& recv_tor, std::vector<double>& buffer_dest, int slice_num){
    // mid rack and dst rack are connected for sure
    int slice = slice_num % num_slots;
    auto dst_hosts = cluster.getNodeIDsForTOR(recv_tor.tor_id);
    int dst_rack = (int) recv_tor.tor_id;

    double  max_tor2tor  = transmit_slot_time * link_speed;
    double  max_host2tor = total_slot_time    * link_speed;

    for(int src_rack = 0; src_rack < buffer_dest.size(); src_rack++){
        double buf_val = buffer_dest[src_rack];
        if(buf_val > 0){

            #if DEBUG
            std::cout << "mid_tor:" << +send_tor.tor_id << "->" << "dst_tor:" << +recv_tor.tor_id << "\n";            
            #endif
            
            //we need to deal with flow-level statistics
            uint64_t src_rackid = static_cast<uint64_t>(src_rack);
            auto src_hosts = cluster.getNodeIDsForTOR(src_rackid);
            auto link_vec = GetInterTORLinkIds(send_tor.tor_id , recv_tor.tor_id);
            int tor_channel = GetFlowTORChannel(send_tor.tor_id, recv_tor.tor_id, slice);
            double tor_capacity = getLinkCapacity(link_vec, tor_channel, slice);
            // -> update buffered_flows by flowid
            bool isLocalFlow = false;
            double total_size = deliverTORFlows(send_tor, src_hosts, dst_hosts, tor_capacity, tor_channel, isLocalFlow, slice);
            // -> update buffer_matrix
            double remain_val = send_tor.getBufferValue(src_rack, dst_rack) - total_size;
            if(remain_val > 0){
                #if DEBUG
                std::cout << "something wrong with remain buffer on tor\n";
                std::cout << "remain:" << remain_val << "\n";
                #endif
            }
            else{ // remain_val <= 0
                remain_val = 0;
            }
            send_tor.updateBufferValue(src_rack, dst_rack, 0); 
            //0 should be changed to remain_val once deliverTORFlows is verified
        }
    }
}


void SingleLayerRotorSimulator::RotorDirectTransfer(TORSwitch& src_tor, TORSwitch& dst_tor, int slice_num){
    int slice = slice_num % num_slots;
    int dst_rack = (int) dst_tor.tor_id;

    //double max_tor2tor  = transmit_slot_time * link_speed;
    //getLinkCapacity()

    double dir_val = src_tor.getLocalValue(dst_rack);
    if(dir_val > 0){

        #if DEBUG
        std::cout << "src_tor:" << +src_tor.tor_id << "->" << "dst_tor:" << +dst_tor.tor_id << "\n";
        #endif

        auto link_vec = GetInterTORLinkIds(src_tor.tor_id, dst_tor.tor_id);
        int tor_channel = GetFlowTORChannel(src_tor.tor_id, dst_tor.tor_id, slice);
        double tor_capacity = getLinkCapacity(link_vec, tor_channel, slice);
        auto dst_hosts = cluster.getNodeIDsForTOR(dst_tor.tor_id);
        auto src_hosts = cluster.getNodeIDsForTOR(src_tor.tor_id);
        bool isLocalFlows = true;
        double total_flowsize = deliverTORFlows(src_tor, src_hosts, dst_hosts, tor_capacity, tor_channel, isLocalFlows, slice);
        //TODO: total_flowsize should be equal to max_tor2tor
        src_tor.updateLocalValue(dst_rack, dir_val - total_flowsize);
    }
    //src_tor.printLocalVector();
}

// recv_offers will be updated
void SingleLayerRotorSimulator::RotorSourceProposal(int src_torid, std::vector<int> tor_list,int slice_num){
    int slice = slice_num % num_slots;

    #if DEBUG
    std::cout << "-----------------\n" ;
    std::cout << "mid_tor:" << +src_torid << "\n";
    std::cout << "proposing to tor:" ; //<< tor_list[0] << "," << tor_list[1] << "," << tor_list[2] << "," << tor_list[3] << "\n";
    #endif

    TORSwitch& src_tor= cluster.tors.at(static_cast<uint64_t>(src_torid));
    auto dir_vec = src_tor.getLocalVector();
    //src_tor.printLocalVector();
    for(int mid_torid: tor_list){
        if(src_torid == mid_torid){
            continue;
        }
        #if DEBUG
        std::cout << mid_torid << "," ;
        #endif
        //std::cout << " po tor:" 
        TORSwitch& mid_tor= cluster.tors.at(static_cast<uint64_t>(mid_torid));
        // 1. update "offer"
        // assign dir_vec to a row of recv_offers on a connected tor, mid_tor
        mid_tor.recv_offers[src_torid] = dir_vec;
        // 2. update "capacity"
        int tor_channel = GetFlowTORChannel(src_tor.tor_id, mid_tor.tor_id, slice);
        auto link_vec = GetInterTORLinkIds(src_tor.tor_id, mid_tor.tor_id);
        auto tor_rotor_linkid = link_vec[0]; // TOR->ROTOR
        double send_cap = links.at(tor_rotor_linkid).getAvailCapacity(tor_channel, slice);
        mid_tor.send_capacity[src_torid] = send_cap;         
    }
    #if DEBUG
    std::cout << "\n-----------------\n" ;
    #endif
    // no local_flows and buffer_flows update in this function 
}

/*
    avail    <- max_bytes_of_next_matching - remain local data
    offer[i] <- offer[i] if avail[i] != 0
    offerscl <- fairshare1d of capacity over offer
    while offerscl has nonzero columns{
        for nonzero column i : offerscl{
             tmpfs <- fairshare1d of avail[i] over offerscl[i]
             avail[i] = avail[i] - sum(tmpfs)
             indir = tmpfs
        } 
    }
*/
void SingleLayerRotorSimulator::RotorCalculateAcceptance(int mid_torid, std::vector<int> tor_list, int slice_num){
    int slice = slice_num % num_slots;
    TORSwitch& mid_tor= cluster.tors.at(static_cast<uint64_t>(mid_torid));

    #if DEBUG
    std::cout << "-----------------\n" ;
    std::cout << "mid_tor:" << +mid_torid << "\n";
    std::cout << "-> tor:" << tor_list[0] << "," << tor_list[1] << "," << tor_list[2] << "," << tor_list[3] << "\n";
    std::cout << "-----------------\n" ;
    #endif

    // this needs to be fixed?-> NOPE 
    // calculate avail, the capacity for mid tors to send data at the next matching
    // so it should have all the available bandwidth!
    // The amount of non-local traffic it can accept per destination is equal to 
    double  max_tor2tor  = transmit_slot_time * link_speed;
    //Iterate over destinations!
    for(uint64_t dst= 0; dst < mid_tor.local_vector.size(); dst++){
        //TODO? intra-flow capacity?

        if(dst == mid_tor.tor_id){
            continue;
        }
        // max_tor2tor - sum(column dst of mid_tor.buffer_matrix) - mid_tor.local_vector[dst]
        double remain_val = mid_tor.local_vector[dst] + mid_tor.getColSumBuffer(dst);
        remain_val = max_tor2tor - remain_val;
        if(remain_val > 0){
            mid_tor.recv_avail[dst] = remain_val;
        }
        else{
            mid_tor.recv_avail[dst] = 0;
        }
    }
    // column i of offer = column i of offer if avail[i]!=0
    // reject these offers, it's impossible to deliver these data
    for(int dst = 0; dst < mid_tor.recv_avail.size(); dst++){
        if(mid_tor.recv_avail[dst] != 0){
            //zeros out the column in the corresponding recv_offers matrix!
            for(int src = 0; src < mid_tor.recv_offers.size(); src++){
                mid_tor.offerscl[src][dst] = mid_tor.recv_offers[src][dst];
            }
        }
    }


    //calculate offerscl by fairsharing offerscl (2d matrix) over capacity (a vector)
    for(int send = 0; send < mid_tor.send_capacity.size(); send++){
        mid_tor.offerscl[send] = util::fairshare1d(mid_tor.offerscl[send], mid_tor.send_capacity[send], true);
    }

    #if DEBUG
    util::printMatrix(mid_tor.recv_offers, "recv_offers");
    util::printMatrix(mid_tor.offerscl, "offerscl");
    util::printVector(mid_tor.send_capacity, "send_capacity");
    util::printVector(mid_tor.recv_avail, "recv_avail");
    #endif

    //while loop, calculate indir
    std::vector<int> nonzero_columns;
    // we have a column checking func for each tor: nonzeroOffersclColumns
    bool hasNonzeroColumns = mid_tor.nonzeroOffersclColumns(nonzero_columns);
    //util::printVector(nonzero_columns, "offerscl_columns");

    while(hasNonzeroColumns){
        for(int col_index: nonzero_columns){
            std::vector<double> col_vec(mid_tor.offerscl[0].size());
            mid_tor.getOffersclColumn(col_index, col_vec);
            //fairshare of avail[col_index] over col_vec, i.e. offerscl_column_i
            col_vec = util::fairshare1d(col_vec, mid_tor.recv_avail[col_index], true);
            double sum_tmpfs = std::accumulate(col_vec.begin(), col_vec.end(), 0.0);
            // avail[i] = avail[i] -sum(tmpfs)
            mid_tor.recv_avail[col_index] = mid_tor.recv_avail[col_index] - sum_tmpfs;
            // column col_index of indir <- col_vec
            mid_tor.updateIndirColumn(col_index, col_vec);
        }
        // offerscl = offer - indir (2d matirx operation)
        for(int row = 0; row < mid_tor.offerscl.size(); row++){
            for(int col = 0; col < mid_tor.offerscl[0].size(); col++){
                mid_tor.offerscl[row][col] = mid_tor.recv_offers[row][col] - mid_tor.indir[row][col];
            }
        }            
        // send_capacity = send_capacity - per_row_sum of indir, all of them are 1d vectors
        int cap_size = mid_tor.send_capacity.size();
        std::vector<double> indir_rowsum(cap_size);
        mid_tor.perRowSumIndir(indir_rowsum);
        for(int i = 0; i < cap_size; i++){
            mid_tor.send_capacity[i] =  mid_tor.send_capacity[i] - indir_rowsum[i];
        }
        // fairshare of tmplc (send_capacity now) over offerscl (2d matrix)
        for(int send = 0; send < mid_tor.send_capacity.size(); send++){
            mid_tor.offerscl[send] = util::fairshare1d(mid_tor.offerscl[send], mid_tor.send_capacity[send], true);
        }

        #if DEBUG
        util::printVector(mid_tor.send_capacity, "while_loop_send_capacity");
        util::printVector(mid_tor.recv_avail, "while_loop_recv_avail");
        util::printMatrix(mid_tor.offerscl, "while_loop_offerscl");
        util::printMatrix(mid_tor.indir, "while_loop_indir");
        #endif

        // checking for the next loop 
        //mid_tor.printOfferscl();
        nonzero_columns.clear();
        hasNonzeroColumns = mid_tor.nonzeroOffersclColumns(nonzero_columns);
        //std::cout <<"hasNonzeroColumns:"<< hasNonzeroColumns <<"\n";
    }

    #if DEBUG
    std::cout << "end while loop\n" ;
    util::printMatrix(mid_tor.indir, "indir");
    #endif

    // update indir to connected tors
    for(int src_torid: tor_list){
        if(src_torid == mid_torid){
            continue;
        }
        TORSwitch& src_tor= cluster.tors.at(static_cast<uint64_t>(src_torid));
        double indir_rowsum = 0;
        for(int dst = 0; dst < mid_tor.indir[0].size(); dst++){
            indir_rowsum += mid_tor.indir[src_torid][dst]; //just a row per src
        }
        src_tor.forward[mid_torid] += indir_rowsum;
        for(int dst = 0; dst < mid_tor.indir[0].size(); dst++){
            src_tor.accept[dst] += mid_tor.indir[src_torid][dst];
        }

    }
}

// TODO: check correntness
double SingleLayerRotorSimulator::updateTORFlows(TORSwitch& src_tor, TORSwitch& mid_tor, double src2mid_capcity, int channel, int slice){
    //int slice = slice_num % num_slots;
    // TODO:
    // update buffer flow on mid_tor: add flows
    // src_tor dercrement local flow
    // mid_tor incerment buffer flow
    auto src_hosts = cluster.getNodeIDsForTOR(src_tor.tor_id);
    std::vector<Flow> flows;
    std::vector<double> flowsize_list;
    for(int dst_rack = 0; dst_rack < src_tor.accept.size(); dst_rack++){
        double buf_val = mid_tor.buffer_matrix[src_tor.tor_id][dst_rack];
        if(buf_val > 0){
            uint64_t dst_rackid = static_cast<uint64_t>(dst_rack);
            auto dst_hosts = cluster.getNodeIDsForTOR(dst_rackid);
            for(auto src_id: src_hosts){
                for(auto dst_id: dst_hosts){
                    auto key_tuple = std::make_tuple(src_id, dst_id);
                    auto flow_found = flow_map.find(key_tuple);
                    if (flow_found != flow_map.end()) {
                        auto flow_vec = flow_found->second;
                        for (auto &flow: flow_vec) {
                            flows.push_back(flow);
                            double remaining_bytes = 0;
                            remaining_bytes = src_tor.getLocalFlowByID(flow.flow_id);
                            flowsize_list.push_back(remaining_bytes); // remaining flow size on this host
                        }
                    }
                }
            }
        }
    }

    auto flowupdates = util::fairshare1d(flowsize_list, src2mid_capcity, true);

    double total_flowsize = 0;
    for(int index = 0 ; index < flows.size(); index++){
        auto flow = flows[index];
        double updated_size = flowupdates[index];
        total_flowsize += updated_size;
        src_tor.decrementLocalFlow(flow.flow_id, updated_size);
        // TODO: check this!
        mid_tor.addBufferFlow(flow);
        mid_tor.incrementBufferFlow(flow.flow_id, updated_size);
        // check if update_size no greater than the bottlebeck on links of the flow
        double valid_update = checkLinkCapacity(flow, channel, updated_size, slice);
        //update the capacity for used links with valid update size
        updateLinkCapacity(flow, channel, valid_update, slice);
    } 
}

void SingleLayerRotorSimulator::RotorIndirection(TORSwitch& src_tor, TORSwitch& mid_tor, int slice_num){
    int slice = slice_num % num_slots;
    // 1. update buffer_matrix for mid tor: get accept matrix from src_tor
    // 2. update local vector for src_tor
    // indir[i[] <- min(indiri , locali)
    // written as updating min(accept, local_vector) to buffer_matrix

    int src_id = src_tor.tor_id; 
    for(int dst = 0; dst < src_tor.accept.size(); dst++){
        //update local vector of src_tor
        double remain_local = src_tor.local_vector[dst] - src_tor.accept[dst];
        if(remain_local >= 0){ // local vector >= accept
            //mid_tor.buffer_matrix[src_id][dst] = mid_tor.buffer_matrix[src_id][dst] + src_tor.accept[dst];
            src_tor.local_vector[dst] = remain_local;        
        }
        else{ // remain_local < 0, i.e. local vector < accept
            //mid_tor.buffer_matrix[src_id][dst] = mid_tor.buffer_matrix[src_id][dst] + src_tor.local_vector[dst];
            src_tor.local_vector[dst] = 0;
        }
        //update buffer matrix of mid_tor, indir value is added here 
        mid_tor.buffer_matrix[src_id][dst] = mid_tor.buffer_matrix[src_id][dst] + mid_tor.indir[src_id][dst];
    }

    // update buffer_flows of the tor, I mean for all flows passed through this tor!
    // src_tor dercrement local flow
    // update buffer flow on mid_tor: add flows
    int tor_channel = GetFlowTORChannel(src_tor.tor_id, mid_tor.tor_id, slice);
    updateTORFlows(src_tor, mid_tor, src_tor.forward[mid_tor.tor_id], tor_channel, slice);

    //clear accept and forward
    //
    std::fill(src_tor.accept.begin(), src_tor.accept.end(), 0.0);
    std::fill(src_tor.forward.begin(), src_tor.forward.end(), 0.0);
    for(int tor = 0; tor < src_tor.buffer_matrix.size() ; tor++){
        std::fill(src_tor.recv_offers[tor].begin(), src_tor.recv_offers[tor].end(), 0.0);
        std::fill(src_tor.indir[tor].begin(), src_tor.indir[tor].end(), 0.0);
        std::fill(src_tor.offerscl[tor].begin(), src_tor.offerscl[tor].end(), 0.0);
    }
}




void SingleLayerRotorSimulator::UpdateLinkDemand(){
    //clear link stats
    for(auto map_iter = links.begin(); map_iter != links.end(); ++map_iter){
        map_iter->second.ResetFlowStats();
    }

    //clear tor stats
    for(auto map_iter = cluster.tors.begin(); map_iter != cluster.tors.end(); ++map_iter){ // sweep mid nodes
        uint64_t tor_id = map_iter->first;
        auto& tor = cluster.tors[tor_id];
        tor.clearFlowMatrices(cluster.tors.size());
        tor.clearFlowStats();
    }

    //clear topo stats
    for(int index = 0; index < cluster.tors.size(); index ++){ 
         std::fill(flow_count_matrix[index].begin(), flow_count_matrix[index].end(), 0);
    }
    deliver_rate_matrix.clear();
    first_cycle_rate.clear();
    flow_map.clear();

    //Init topo stats per flow
    for (const auto &flow: flows) {
        if (!flow.HasStarted(time_now) || flow.IsCompleted()){
            continue;
        }
        //use for check intra-tor flows
        uint64_t src_rack = GetRackId(flow.src_host);
        uint64_t dst_rack = GetRackId(flow.dst_host);
        flow_count_matrix[src_rack][dst_rack]++;
        auto vec = std::vector<double>(num_slots); 
        std::fill(vec. begin(),vec.end(), 0.0);
        deliver_rate_matrix[flow.flow_id] = vec;
        first_cycle_rate[flow.flow_id] = vec; 
        flow_map[std::make_tuple(flow.src_host, flow.dst_host)].push_back(flow);
        //TODO: per-tor local-only flows: flows that src and dst are both in the same tor
        //we will need a data structure to store this on each tor? ->just a vector of flows?    
    }

    //Init tor stats per flow
    for (auto &flow: flows) {
        bool stopped_flow = false;
        if (!flow.HasStarted(time_now) || flow.IsCompleted()){
            stopped_flow = true;
        }
        for(auto tor_iter = cluster.tors.begin(); tor_iter != cluster.tors.end(); ++tor_iter){
            TORSwitch& tor = tor_iter->second;
            tor.initLocalFlow(flow, stopped_flow);
        }
    }

    // to warm up the first cycle, we need to do 2 or more cycles, at least for now?
    for(int slice = 0; slice < num_slots*2; slice++){
        if(slice == num_slots){
            for(auto map_iter = links.begin(); map_iter != links.end(); ++map_iter){
                map_iter->second.ResetFlowStats();
            }
            for (auto &flow: flows) {
                if (!flow.HasStarted(time_now) || flow.IsCompleted()){
                    continue;
                }
                first_cycle_rate[flow.flow_id] = deliver_rate_matrix[flow.flow_id];
                auto vec = std::vector<double>(num_slots); 
                std::fill(vec. begin(),vec.end(), 0.0);
                deliver_rate_matrix[flow.flow_id] = vec;
            }
        }

        #if DEBUG
        std::cout << "---------------Slice:" << slice <<"---------------------\n";
        printVectorPerPhase("pre-phase-1-step1");
        std::cout << "\n-----------------\nRotorBufferedTransfer\n-----------------\n";
        #endif

        /* PHASE-1, STEP-1 sending buffered, non-local traffic directly to destination racks */
        for(auto tor_iter = cluster.tors.begin(); tor_iter != cluster.tors.end(); ++tor_iter){
            uint64_t dst_torid = tor_iter->first;
            int dst_intid = static_cast<int>(dst_torid);
            TORSwitch& dst_tor = tor_iter->second;
            if (HasIncomingFlows(dst_torid) == true){
                std::vector<int> mid_tor_list = GetConnectedTORs(dst_torid, slice);
                for(auto mid_torid: mid_tor_list){
                    if(mid_torid == dst_intid){
                        continue;
                    }
                    TORSwitch& mid_tor= cluster.tors.at(static_cast<uint64_t>(mid_torid));
                    std::vector<double> buffer_dest(cluster.tors.size());
                    mid_tor.getBufferColumn(dst_intid, buffer_dest);
                    double buf_val = std::accumulate(buffer_dest.begin(), buffer_dest.end(), 0.0);
                    if(buf_val > 0.0){
                        //flow stats are based on hosts, so it would be handled in the BufferedTransfer
                        RotorBufferedTransfer(mid_tor, dst_tor, buffer_dest, slice);
                    }
                }
            }
        }

        #if DEBUG
        printVectorPerPhase("post-phase-1-step-1");
        std::cout << "\n-----------------\nRotorDirectTransfer\n-----------------\n";
        #endif

        /* PHASE-1, STEP-2: sending local traffic directly to destination racks */
        for(auto tor_iter = cluster.tors.begin(); tor_iter != cluster.tors.end(); ++tor_iter){
            uint64_t src_torid = tor_iter->first;
            TORSwitch& src_tor = tor_iter->second;
            std::vector<int> tor_list = GetConnectedTORs(src_torid, slice);
            for(auto dst_torid: tor_list){
                if(src_torid == dst_torid){
                    continue;
                }
                if(src_tor.getLocalValue(dst_torid) > 0.0){
                    TORSwitch& dst_tor= cluster.tors.at(static_cast<uint64_t>(dst_torid));
                    //flow stats are based on hosts, so it would be handled in the DirectTransfer
                    RotorDirectTransfer(src_tor, dst_tor, slice);
                }
            }
        }

        #if DEBUG
        printVectorPerPhase("post-phase-1-step-2");
        /* PHASE-1, STEP-3: proposing traffic to connected rack */
        std::cout << "\n-----------------\nRotorSourceProposal\n-----------------\n";
        #endif

        for(auto tor_iter = cluster.tors.begin(); tor_iter != cluster.tors.end(); ++tor_iter){
            uint64_t src_torid = tor_iter->first;
            TORSwitch& src_tor = tor_iter->second;
            std::vector<int> tor_list = GetConnectedTORs(src_torid, slice);
            auto dir_vec =src_tor.getLocalVector();
            double dir_val = std::accumulate(dir_vec.begin(), dir_vec.end(), 0.0);
            //for(auto mid_torid: tor_list){
            if(dir_val > 0.0){
                //TORSwitch& mid_tor= cluster.tors.at(static_cast<uint64_t>(mid_torid));
                RotorSourceProposal(static_cast<int>(src_torid), tor_list, slice);
            }
        }

        #if DEBUG
        printVectorPerPhase("pre-phase-2");
        std::cout << "-----------------\nRotorCalculateAcceptance\n-----------------\n";
        #endif

        /* PHASE-2: mid racks calculate accepted traffic from remote racks */
        for(auto tor_iter = cluster.tors.begin(); tor_iter != cluster.tors.end(); ++tor_iter){
            uint64_t mid_torid = tor_iter->first;
            std::vector<int> tor_list = GetConnectedTORs(mid_torid, slice);
            //std::cout << +mid_torid << " is connected to tor:" << tor_list[0] << "," << tor_list[1] << "," << tor_list[2] << "," << tor_list[3] << "\n";
            //std::cout << "RotorCalculateAcceptance on mid_tor" << mid_torid << "\n";
            RotorCalculateAcceptance(mid_torid, tor_list, slice);
        }

        /* PHASE-3: src racks transfer to remote racks */
        #if DEBUG
        std::cout << "\n-----------------\nRotorIndirection\n-----------------\n";
        #endif

        for(auto tor_iter = cluster.tors.begin(); tor_iter != cluster.tors.end(); ++tor_iter){
            uint64_t src_torid = tor_iter->first;
            TORSwitch& src_tor = tor_iter->second;
            std::vector<int> tor_list = GetConnectedTORs(src_torid, slice);

            #if DEBUG
            std::cout << "src_tor:" << src_torid << "\n";
            util::printVector(src_tor.accept, "accept");
            util::printVector(src_tor.forward,"forward");
            #endif

            for(auto mid_torid: tor_list){
                if(src_torid == mid_torid){
                    continue;
                }
                TORSwitch& mid_tor= cluster.tors.at(static_cast<uint64_t>(mid_torid));
                // recv_offers is now the recv indir
                #if DEBUG
                std::cout << "Indir on src_tor:" << src_torid << ",mid_tor" << mid_torid << "\n";
                #endif

                RotorIndirection(src_tor, mid_tor, slice);
            }
        }
        #if DEBUG
        printVectorPerPhase("post-phase-3");
        #endif
    }

    #if DEBUG
    std::cout << "first_cycle_rate:\n";
    for (const auto &flow: flows) {
        if (!flow.HasStarted(time_now) || flow.IsCompleted()){
            continue;
        }
        auto f_matrix = first_cycle_rate[flow.flow_id];
        std::cout<< "flow:"<<  +flow.src_host << "->"<< +flow.dst_host <<"\n";
        std::cout << "slice:";
        double first_cycle_unit_100gbps = 0;
        for(int slice = 0; slice < num_slots; slice++){
            if(f_matrix[slice]>0)
                std::cout << slice << ":" <<  f_matrix[slice] << ",";
            first_cycle_unit_100gbps += f_matrix[slice]/12500000000.0;  
        }
        std::cout <<"\n";
        std::cout << "num of 100 gbps units:" << first_cycle_unit_100gbps << "\n";
    }

    std::cout << "other_cycle_rate:\n";
    for (const auto &flow: flows) {
        if (!flow.HasStarted(time_now) || flow.IsCompleted()){
            continue;
        }
        auto d_matrix = deliver_rate_matrix[flow.flow_id];
        //auto f_matrix = first_cycle_rate[flow.flow_id];
        std::cout<< "flow:"<<  +flow.src_host << "->"<< +flow.dst_host <<"\n";
        //double first_cycle_unit_100gbps = 0;
        double other_cycle_unit_100gbps = 0;
        std::cout << "slice:" ;
        for(int slice = 0; slice < num_slots; slice++){
            if(d_matrix[slice]>0)
                std::cout << slice << ":" <<  d_matrix[slice] << ",";
            other_cycle_unit_100gbps += d_matrix[slice]/12500000000.0;  
        }
        std::cout <<"\n";
        std::cout << "num of 100 gbps units:" << other_cycle_unit_100gbps << "\n";
    }
    #endif
    /*fprintf(stderr, "Scanning %zu flows to update demand ...\n", flows.size());*/
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

