#include <utility>
#include <tuple>
#include <iostream>
#include <vector>
#include "flowsim_topo_rotor.h"

void SingleLayerRotorSimulator::InitializeLinks(){
    for (uint64_t host_id = 0; host_id < rack_count * hosts_per_rack; host_id++) {
        links.emplace(std::make_pair(std::make_tuple(HOST_TOR, host_id), Link(link_speed, channel_count)));
        links.emplace(std::make_pair(std::make_tuple(TOR_HOST, host_id), Link(link_speed, channel_count)));
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
        links.at(link_id).IncrementFlowCount(); 
    }   
}

double SingleLayerRotorSimulator::GetFlowRemainingTime(const Flow &flow) const{    
    auto current_rate = GetFlowRate(flow);
    return flow.GetRemainingTime(current_rate);
}

uint64_t SingleLayerRotorSimulator::GetTransmittedBytes(const Flow &flow, double interval) const{
    return (uint64_t)(GetFlowRate(flow) * interval);      
}

std::vector<double> SingleLayerRotorSimulator::GetRatesPerChannel(const Flow &flow) const{
}

std::vector<uint64_t> SingleLayerRotorSimulator::GetBytesPerChannel(const Flow &flow, std::vector <double> &rates) const{
}

void SingleLayerRotorSimulator::InitializeMatchingMatrix(){    
}

int SingleLayerRotorSimulator::GetFlowChannel(const Flow &flow) const{
}

int SingleLayerRotorSimulator::GetFlowChannel(uint64_t src, uint64_t dst) const{
}

double SingleLayerRotorSimulator::GetFlowRateForDownRotors(const Flow &flow) const{    
}

void SingleLayerRotorSimulator::UpdateLinkDemand(){
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
}

