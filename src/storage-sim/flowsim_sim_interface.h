#ifndef FLOW_SIM_ISIMULATOR_H
#define FLOW_SIM_ISIMULATOR_H

#include <utility>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <ctime>
#include <cinttypes>
#include <cmath>
#include <iostream>

#include <algorithm>
#include <limits>
#include <map>
#include <mutex>
#include <string>
#include <tuple>
#include <vector>

#include "flowsim_config_macro.h"
#include "simple_cluster.h"
#include "parallel_for.h"
#include "flow.h"
#include "link.h"
#include "task.h"

//using namespace std;

class ISimulator {
private:
    std::mutex t_mutex;
    virtual void InitializeLinks() = 0;
    virtual std::vector<link_id_t> GetLinkIds(const Flow &flow) const = 0;
    //virtual void IncrementLinkFlowCount(const Flow &flow) = 0;
    virtual double GetFlowRemainingTime(const Flow &flow) const = 0;
    virtual uint64_t GetTransmittedBytes(const Flow &flow, double interval) const = 0;

    // Updates the link demand and find next flow to start/end
    // @return whether there's a new event.
    virtual void UpdateLinkDemand() = 0;

    bool GetNextFlowEvent() {
        min_next_time = std::numeric_limits<double>::max();
        flow_next = nullptr;
        parallel_for(flows.size(), [this](size_t start, size_t end) {
            double local_min_next_time = std::numeric_limits<double>::max();
            Flow *local_flow_next = nullptr;
            for (size_t i = start; i < end; i++) {
                auto &flow = flows[i];
                double next_time = std::numeric_limits<double>::max();

                if (!flow.HasStarted(time_now)) {   // Upcoming flow
                    // Check for earliest arrival flow
                    std::cout << std::fixed << "Check for earliest arrival flow, ";
                    next_time = flow.time_start - time_now;
                    std::cout << std::fixed << "Flow Arrival Time:" << next_time << "\n";
                } else if (!flow.IsCompleted()) {   // Existing flow
                    // Check for earliest finish
                    std::cout << std::fixed << "Check for earliest finish, flow " ;
                    next_time = GetFlowRemainingTime(flow);
                    //std::cout << "----------------\n" ;
                    std::cout << flow.src_host << "->" << flow.dst_host << "\n";
                    std::cout << std::fixed << "Flow Remaining Time:" << next_time;
                    std::cout << "\n----------------\n";
                } else {
                    continue;
                }

                // fprintf(stderr, "%" PRIu64 "->%" PRIu64 ": rate: %f, time remaining: %f\n",
                //         flow.host_uplink, flow.host_downlink, flow.current_rate, next_time);
                if (next_time < local_min_next_time) {
                    std::cout << "next_time:" << next_time << "\n";
                    local_min_next_time = next_time;
                    local_flow_next = &flow;
                }
            }

            std::lock_guard<std::mutex> lock(t_mutex);
            if (local_min_next_time < min_next_time) {
                min_next_time = local_min_next_time;
                flow_next = local_flow_next;
            }
        });

        std::cout << "the next flow" << flow_next->src_host << "->" << flow_next->dst_host << "\n";

        return flow_next != nullptr;
    }

    void RunToNextEvent() {
        if (flow_next == nullptr)
            return;

        double interval = min_next_time;
        double time_end = time_now + interval;
        printf("Running sim for %f time.\n", interval);
        parallel_for(flows.size(), [this, interval, time_end](size_t start, size_t end) {
            for (size_t i = start; i < end; i++) {
                auto &flow = flows[i];
                if (flow.HasStarted(time_now) && !flow.IsCompleted()) {
                    uint64_t transmitted_bytes = GetTransmittedBytes(flow, interval);
                    flow.completed_bytes += transmitted_bytes;
                    if (flow.IsCompleted())
                        flow.time_end = time_end;
                }
            }
        });

        time_now = time_end;
    }

protected:
    SimpleCluster cluster;
    uint64_t rack_count;
    uint64_t hosts_per_rack;
    double link_speed;

    double time_now;      // Current simulation time
    Flow *flow_next;        // Next flow to start or end
    double min_next_time;   // Time till next event

    std::map<link_id_t, Link> links;

    std::vector<Flow> flows;
    std::map<size_t, std::vector<const Flow *>> all_completed_flows;

    uint64_t GetRackId(uint64_t host_id) {
        return host_id / hosts_per_rack;
    }

    double checkLinkCapacity(const Flow &flow, int channel, double proposed_capacity, int slice){
        double min_cap = std::numeric_limits<double>::max();
        for (auto &link_id: GetLinkIds(flow)) {
            double cap = links.at(link_id).getAvailCapacity(channel, slice);
            if( cap < min_cap){
                min_cap = cap;
            }
        }

        if(min_cap < proposed_capacity){
            return min_cap;
        }
        else{
            return proposed_capacity;
        }
    }

    void updateLinkCapacity(const Flow &flow, int channel, double used_capacity, int slice){
        for (auto &link_id: GetLinkIds(flow)) {
            links.at(link_id).ReduceAvailCapacity(channel, slice, used_capacity);
        }
    }

    double getLinkCapacity(std::vector<link_id_t> link_vec, int channel, int slice){
        std::vector<double> link_cap;
        for (auto &link_id: link_vec) {
            double cap = links.at(link_id).getAvailCapacity(channel, slice);
            link_cap.push_back(cap);
        }        
        return *min_element(link_cap.begin(), link_cap.end());
    }

    //MAYBE: fix with host-ToR-RS
    double GetFlowRate(const Flow &flow, std::tuple<int, int> channels, int slot) const {
        std::vector<double> rates;
        int src_channel = std::get<0>(channels);
        int dst_channel = std::get<1>(channels);  
        for (const auto &link_id: GetLinkIds(flow)) {
            LinkType linktype = std::get<0>(link_id);
            double rate;
            if(linktype == HOST_TOR && src_channel >= 0  && src_channel < HOST_CHANNELS){
                rate = links.at(link_id).GetRatePerFlow(src_channel, slot); 
                //std::cout << std::fixed << "HOST_TOR rate" << rate << "\n";
            }
            else if (linktype == TOR_HOST && dst_channel >= 0 && dst_channel < HOST_CHANNELS){
                rate = links.at(link_id).GetRatePerFlow(dst_channel, slot);
                //std::cout << std::fixed << "TOR_HOST rate" << rate << "\n";
            }
            else{
                //std::cout << "we don't have a valid channel and link type\n";
                rate = links.at(link_id).GetRatePerFlow(-1, slot); // a fallback case
            }
            rates.push_back(rate);
        }
        return *min_element(rates.begin(), rates.end());
    }


    double GetFlowRate(const Flow &flow, int slot ,int channel = -1) const {
        std::vector<double> rates;
        for (const auto &link_id: GetLinkIds(flow)) {
            double rate = links.at(link_id).GetRatePerFlow(channel, slot);
            rates.push_back(rate);
        }
        return *min_element(rates.begin(), rates.end());
    }

public:
    ISimulator(SimpleCluster cluster)
        : cluster(cluster), rack_count(cluster.rack_count),
          hosts_per_rack(cluster.nodes_per_rack), link_speed(cluster.link_speed),
          time_now(0), flow_next(nullptr) {}

    double GetCurrentTime() {
        return time_now;
    }

    const Flow * AddFlow(uint64_t src, uint64_t dst, uint64_t size, double time, Task *task) {
        uint64_t src_rack = GetRackId(src);
        uint64_t dst_rack = GetRackId(dst);
        Flow flow(src, src_rack, dst, dst_rack, size, time, task);
        this->flows.push_back(flow);
        return &(this->flows.back());
    }

    void printFlows(){
        for(Flow f: flows){
            if (!f.HasStarted(time_now) || f.IsCompleted()){
                continue;
            }
            std::cout << "flowid" << f.flow_id <<"src:"<< f.src_host <<", dst:" << f.dst_host << "\n";
        }
    }

    std::vector<const Flow *> RunToNextCompletion() {
        UpdateLinkDemand();

        //TODO: get rid of this when it's done
        //std::exit(0);

        if (!GetNextFlowEvent()){
            std::cout<< "no next flow event\n";
            return std::vector<const Flow *>();
        }

        double interval = min_next_time;
        double time_end = time_now + interval;
        //fprintf(stderr, "running_sim_for %f time.\n", interval);
        std::cout<< "running_sim_for "<<  interval << " time\n";
        parallel_for(flows.size(), [this, interval, time_end](size_t start, size_t end) {
            std::vector<const Flow *> completed_flows;
            for (size_t i = start; i < end; i++) {
                auto &flow = flows[i];
                if (flow.HasStarted(time_now) && !flow.IsCompleted()) {
                    std::cout << "flowid" << flow.flow_id <<"src:"<< flow.src_host <<", dst:" << flow.dst_host << "\n";
                    uint64_t transmitted_bytes = GetTransmittedBytes(flow, interval);
                    //std::cout << "transmits"<< +transmitted_bytes <<"\n";
                    flow.completed_bytes += transmitted_bytes;
                    if (flow.IsCompleted()) {
                        std::cout << "flowid" << flow.flow_id <<"src:"<< flow.src_host <<", dst:" << flow.dst_host << " is_completed\n";
                        flow.time_end = time_end;
                        completed_flows.push_back(&flow);
                    }
                }
            }

            std::lock_guard<std::mutex> lock(t_mutex);
            all_completed_flows[start] = completed_flows;
        });

        time_now = time_end;
        std::vector<const Flow *> completed_flows;
        for (auto &[_, partial_flows]: all_completed_flows) {
            completed_flows.insert(completed_flows.end(), partial_flows.begin(), partial_flows.end());
        }
        all_completed_flows.clear();
        return completed_flows;
    }

    void RunToCompletion() {
        while (true) {
            UpdateLinkDemand();
            if (!GetNextFlowEvent())
                break;
            RunToNextEvent();
        }
    }

    void PrintFlowCompletionTimes() {
        printf("======== FCT Report ========\n");
        std::map<std::string, std::map<double, size_t>> fcts;
        std::map<std::string, std::map<std::tuple<double, double>, size_t>> start_end;
        for (const auto &flow: flows) {
            start_end[flow.task->tag][std::make_tuple(flow.time_start, flow.time_end)]++;
            double fct = flow.time_end - flow.time_start;
            fcts[flow.task->tag][fct]++;
        }

        printf("Tag,FCT,Count\n");
        for (const auto &[tag, m]: fcts)
            for (const auto &[fct, count]: m) {
                printf("%s,%f,%zu\n", tag.c_str(), fct, count);
            }
        printf("\n");

        printf("Tag,Start,End,Count\n");
        for (const auto &[tag, m]: start_end)
            for (const auto &[tuple, count]: m) {
                printf("%s,%f,%f,%zu\n", tag.c_str(), std::get<0>(tuple), std::get<1>(tuple), count);
            }
/*
        printf("src,dst,bytes,start,end\n");
        for (const auto &flow: flows) {
            printf("%d,%d,%" PRIu64 ",%.3f,%.3f\n",
                    flow.host_uplink, flow.host_downlink,
                    flow.total_bytes, flow.time_start, flow.time_end);
        }
*/
        printf("======== End ========\n");

        flows.clear();
    }
};

#endif  // FLOW_SIM_ISIMULATOR_H
