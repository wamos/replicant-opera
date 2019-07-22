#include <utility>
#include <tuple>
#include <iostream>
#include <vector>
#include "flowsim_topo_test.h"

void TestTopologySimulator::InitializeLinks(){
    for (uint64_t host_id = 0; host_id < rack_count * hosts_per_rack; host_id++) {
        links.emplace(std::make_pair(std::make_tuple(HOST_HOST, host_id), Link(link_speed)));
    }    
}

std::vector<link_id_t> TestTopologySimulator::GetLinkIds(const Flow &flow) const{
    std::vector<link_id_t> linkid_list;
    linkid_list.emplace_back(std::make_tuple(HOST_HOST, flow.src_host));

    return linkid_list;
}

void TestTopologySimulator::IncrementLinkFlowCount(const Flow &flow){
    for (const auto &link_id: GetLinkIds(flow)){
        links.at(link_id).IncrementFlowCount(); 
    }   
}

double TestTopologySimulator::GetFlowRemainingTime(const Flow &flow) const{    
    auto current_rate = GetFlowRate(flow);
    return flow.GetRemainingTime(current_rate);
}

uint64_t TestTopologySimulator::GetTransmittedBytes(const Flow &flow, double interval) const{
    return (uint64_t)(GetFlowRate(flow) * interval);      
}

void TestTopologySimulator::UpdateLinkDemand(){
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

