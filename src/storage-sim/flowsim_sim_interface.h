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
#include <tuple>

#include "parallel_for.h"

using namespace std;

#define KB(n) ((uint64_t)n * 1024)
#define MB(n) ((uint64_t)n * 1024 * 1024)
#define GB(n) ((uint64_t)n * 1024 * 1024 * 1024)
#define Kb(n) ((uint64_t)n * 1024 / 8)
#define Mb(n) ((uint64_t)n * 1024 * 1024 / 8)
#define Gb(n) ((uint64_t)n * 1024 * 1024 * 1024 / 8)

enum LinkType { HOST_TOR, TOR_CORE, CORE_TOR, TOR_HOST, TOR_TOR };
typedef tuple<LinkType, uint64_t> link_id_t;

struct Link {
    double capacity;
    int flow_count; // # of flows always transmitting
    vector<int> flow_counts; // # of flows in TDMA channels

    Link(double capacity, int channel_count = 0)
        : capacity(capacity), flow_count(0), flow_counts(vector<int>(static_cast<unsigned long>(channel_count))) {}

    double GetRatePerFlow(int channel = -1) const {
        auto total_flow_count = (channel >= 0 ? flow_counts[static_cast<unsigned long>(channel)] : 0) + flow_count;
        if (total_flow_count == 0)
            return 0;
        else
            return capacity / total_flow_count;
    }

    void ResetFlowCounts() {
        flow_count = 0;
        fill(flow_counts.begin(), flow_counts.end(), 0);
    }

    void IncrementFlowCount() {
        flow_count++;
    }

    void IncrementFlowCount(int channel) {
        flow_counts[static_cast<unsigned long>(channel)]++;
    }
};

static uint64_t global_task_id = 0;
struct Task {
    uint64_t task_id;
    uint64_t host_id;
    double start_time;
    double end_time;
    vector<uint64_t> flows;
    string type;

    Task(uint64_t host_id, double start_time, string type)
            : host_id(host_id), start_time(start_time), end_time(numeric_limits<double>::max()), type(std::move(type))
    {
        task_id = global_task_id++;
    }
};

struct MapTask : Task {
    uint64_t file_id;
    uint64_t block_id;

    MapTask() : Task(static_cast<uint64_t>(-1), 0, "map"),
            file_id(static_cast<uint64_t>(-1)),
            block_id(static_cast<uint64_t>(-1)) {}

    MapTask(uint64_t host_id, double start_time, uint64_t file_id, uint64_t block_id)
            : Task(host_id, start_time, "map"), file_id(file_id), block_id(block_id) {}
};

struct ReduceTask : Task {
    vector<uint64_t> file_ids;

    ReduceTask() : Task(static_cast<uint64_t>(-1), 0, "reduce") {}

    ReduceTask(uint64_t host_id, double start_time, vector<uint64_t> file_ids, string sub_task = "")
            : Task(host_id, start_time, "reduce" + sub_task), file_ids(file_ids) {}
};

struct HadoopNode {
    map<uint64_t, MapTask *> map_tasks;
    map<uint64_t, ReduceTask *> reduce_tasks;
    map<uint64_t, Task *> completed_tasks;

    HadoopNode() {
    }

    ~HadoopNode() {
        for (auto &[task_id, task]: map_tasks)
            delete task;
        for (auto &[task_id, task]: reduce_tasks)
            delete task;
    }
};

struct HadoopCluster {
    uint64_t rack_count;
    uint64_t nodes_per_rack;
    double link_speed;
    int map_core_count;
    int reduce_core_count;
    map<uint64_t, HadoopNode> hosts;

    HadoopCluster(uint64_t rack_count, uint64_t nodes_per_rack, double link_speed, int map_core_count, int reduce_core_count)
            : rack_count(rack_count),
              nodes_per_rack(nodes_per_rack),
              link_speed(link_speed),
              map_core_count(map_core_count),
              reduce_core_count(reduce_core_count) {
        printf("[Config] Cluster %" PRIu64 " racks, %" PRIu64 " hosts/rack, %f Gbps link, %d maps/host, %d reduces/host.\n",
               rack_count, nodes_per_rack, link_speed / Gb(1), map_core_count, reduce_core_count);
        for (uint64_t rack_id = 0; rack_id < rack_count; rack_id++) {
            for (uint64_t node_index = 0; node_index < nodes_per_rack; node_index++) {
                uint64_t host_id = GetHostId(rack_id, node_index);
                hosts[host_id] = HadoopNode();
            }
        }
    }

    uint64_t GetTotalNodeCount() const {
        return rack_count * nodes_per_rack;
    }

    uint64_t GetRackId(uint64_t node_id) const {
        return node_id / nodes_per_rack;
    }

    uint64_t GetHostId(uint64_t rack_id, uint64_t host_index) const {
        return rack_id * this->nodes_per_rack + host_index;
    }
};

static uint64_t global_flow_id = 0;
struct Flow {
    uint64_t flow_id;
    double time_start;
    double time_end;
    uint64_t total_bytes;
    uint64_t completed_bytes;
//    double current_rate;
    Task *task;

    /* Ids of the links used by the flow */
    uint64_t src_host;  // Source to ToR
    uint64_t dst_host;  // ToR to destination

    Flow(uint64_t src_host, uint64_t src_tor, uint64_t dst_host, uint64_t dst_tor, uint64_t size, double timestamp, Task *task) {
        this->flow_id = global_flow_id++;
        this->time_start = timestamp;
        this->time_end = numeric_limits<double>::max();
        this->total_bytes = size;
        this->completed_bytes = 0;
//        this->current_rate = 0;
        this->task = task;

        this->src_host = src_host;
        this->dst_host = dst_host;
    }

    bool HasStarted(double now) const {
        return this->time_start <= now;
    }

    bool IsCompleted() const {
        return completed_bytes >= total_bytes;
    }

    uint64_t GetRemainingBytes() const {
        return total_bytes - completed_bytes;
    }

    double GetRemainingTime(double current_rate) const {
        if (this->IsCompleted())
            return 0;
        else
            return (double)(total_bytes - completed_bytes) / current_rate;
    }
};

class ISimulator {
private:
    mutex t_mutex;
    virtual void InitializeLinks() = 0;
    virtual vector<link_id_t> GetLinkIds(const Flow &flow) const = 0;
    virtual void IncrementLinkFlowCount(const Flow &flow) = 0;
    virtual double GetFlowRemainingTime(const Flow &flow) const = 0;
    virtual uint64_t GetTransmittedBytes(const Flow &flow, double interval) const = 0;

    // Updates the link demand and find next flow to start/end
    // @return whether there's a new event.
    virtual void UpdateLinkDemand() = 0;

    bool GetNextFlowEvent() {
        min_next_time = numeric_limits<double>::max();
        flow_next = nullptr;
        parallel_for(flows.size(), [this](size_t start, size_t end) {
            double local_min_next_time = numeric_limits<double>::max();
            Flow *local_flow_next = nullptr;
            for (size_t i = start; i < end; i++) {
                auto &flow = flows[i];
                double next_time = numeric_limits<double>::max();

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

            lock_guard lock(t_mutex);
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
    HadoopCluster cluster;
    uint64_t rack_count;
    uint64_t hosts_per_rack;
    double link_speed;

    double time_now;      // Current simulation time
    Flow *flow_next;        // Next flow to start or end
    double min_next_time;   // Time till next event

    map<link_id_t, Link> links;
    vector<Flow> flows;
    map<size_t, vector<const Flow *>> all_completed_flows;

    uint64_t GetRackId(uint64_t host_id) {
        return host_id / hosts_per_rack;
    }

    double GetFlowRate(const Flow &flow, int channel = -1) const {
        vector<double> rates;
        for (const auto &link_id: GetLinkIds(flow)) {
            double rate = links.at(link_id).GetRatePerFlow(channel);
            rates.push_back(rate);
        }

        return *min_element(rates.begin(), rates.end());
    }

public:
    ISimulator(HadoopCluster cluster)
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

    vector<const Flow *> RunToNextCompletion() {
        UpdateLinkDemand();

        if (!GetNextFlowEvent())
            return vector<const Flow *>();

        double interval = min_next_time;
        double time_end = time_now + interval;
        fprintf(stderr, "Running sim for %f time.\n", interval);
        parallel_for(flows.size(), [this, interval, time_end](size_t start, size_t end) {
            vector<const Flow *> completed_flows;
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

            lock_guard lock(t_mutex);
            all_completed_flows[start] = completed_flows;
        });

        time_now = time_end;
        vector<const Flow *> completed_flows;
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
        map<string, map<double, size_t>> fcts;
        map<string, map<tuple<double, double>, size_t>> start_end;
        for (const auto &flow: flows) {
            start_end[flow.task->type][make_tuple(flow.time_start, flow.time_end)]++;
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
                printf("%s,%f,%f,%zu\n", tag.c_str(), get<0>(tuple), get<1>(tuple), count);
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
