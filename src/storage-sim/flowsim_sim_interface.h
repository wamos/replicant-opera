#include <utility>

#ifndef FLOW_SIM_ISIMULATOR_H
#define FLOW_SIM_ISIMULATOR_H

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <ctime>
#include <cinttypes>
#include <cmath>

#include <algorithm>
#include <limits>
#include <map>
#include <mutex>
#include <string>
#include <tuple>
#include <vector>

#include "flowsim_config_macro.h"
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
    virtual void IncrementLinkFlowCount(const Flow &flow) = 0;
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
                    next_time = flow.time_start - time_now;
                } else if (!flow.IsCompleted()) {   // Existing flow
                    // Check for earliest finish
                    next_time = GetFlowRemainingTime(flow);
                } else {
                    continue;
                }

                // fprintf(stderr, "%" PRIu64 "->%" PRIu64 ": rate: %f, time remaining: %f\n",
                //         flow.host_uplink, flow.host_downlink, flow.current_rate, next_time);
                if (next_time < local_min_next_time) {
                    local_min_next_time = next_time;
                    local_flow_next = &flow;
                }
            }

            std::lock_guard lock(t_mutex);
            if (local_min_next_time < min_next_time) {
                min_next_time = local_min_next_time;
                flow_next = local_flow_next;
            }
        });

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

    double GetFlowRate(const Flow &flow, int channel = -1) const {
        std::vector<double> rates;
        for (const auto &link_id: GetLinkIds(flow)) {
            double rate = links.at(link_id).GetRatePerFlow(channel);
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

    std::vector<const Flow *> RunToNextCompletion() {
        UpdateLinkDemand();

        if (!GetNextFlowEvent())
            return std::vector<const Flow *>();

        double interval = min_next_time;
        double time_end = time_now + interval;
        fprintf(stderr, "Running sim for %f time.\n", interval);
        parallel_for(flows.size(), [this, interval, time_end](size_t start, size_t end) {
            std::vector<const Flow *> completed_flows;
            for (size_t i = start; i < end; i++) {
                auto &flow = flows[i];
                if (flow.HasStarted(time_now) && !flow.IsCompleted()) {
                    uint64_t transmitted_bytes = GetTransmittedBytes(flow, interval);
                    flow.completed_bytes += transmitted_bytes;
                    if (flow.IsCompleted()) {
                        flow.time_end = time_end;
                        completed_flows.push_back(&flow);
                    }
                }
            }

            std::lock_guard lock(t_mutex);
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
            start_end[flow.task->type][std::make_tuple(flow.time_start, flow.time_end)]++;
            double fct = flow.time_end - flow.time_start;
            fcts[flow.task->type][fct]++;
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
