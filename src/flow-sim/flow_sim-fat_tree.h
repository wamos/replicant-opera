#ifndef FLOW_SIM_FAT_TREE_H
#define FLOW_SIM_FAT_TREE_H

#include "flow_sim-isimulator.h"

// This is a simplified Fat-Tree simulator. The network core is considered
//  an idealized packet switch, with no over-subscription. ToRs are
//  over-subscribed, i.e. ToR uplink capacity is 1/k * downlink capacity,
//  where k is the over subscription factor.
class FatTreeSimulator : public ISimulator {
private:
    double host_link_speed;
    double tor_link_speed;
    int over_subscription;

    void InitializeLinks() override {
        for (uint64_t host_id = 0; host_id < rack_count * hosts_per_rack; host_id++) {
            uint64_t rack_id = GetRackId(host_id);
            links.emplace(make_pair(make_tuple(HOST_TOR, host_id), Link(host_link_speed)));
            links.emplace(make_pair(make_tuple(TOR_CORE, rack_id), Link(tor_link_speed)));
            links.emplace(make_pair(make_tuple(CORE_TOR, rack_id), Link(tor_link_speed)));
            links.emplace(make_pair(make_tuple(TOR_HOST, host_id), Link(host_link_speed)));
        }
    }

    vector<link_id_t> GetLinkIds(const Flow &flow) const override {
        vector<link_id_t> links;

        links.emplace_back(make_tuple(HOST_TOR, flow.src_host));
        auto src_rack = cluster.GetRackId(flow.src_host);
        auto dst_rack = cluster.GetRackId(flow.dst_host);
        if (src_rack != dst_rack) {
            links.emplace_back(make_tuple(TOR_CORE, src_rack));
            links.emplace_back(make_tuple(CORE_TOR, dst_rack));
        }
        links.emplace_back(make_tuple(TOR_HOST, flow.dst_host));

        return links;
    }

    void IncrementLinkFlowCount(const Flow &flow) override {
        for (const auto &link_id: GetLinkIds(flow))
            links.at(link_id).IncrementFlowCount();
    }

    double GetFlowRemainingTime(const Flow &flow) const override {
        auto current_rate = GetFlowRate(flow);
        return flow.GetRemainingTime(current_rate);
    }

    uint64_t GetTransmittedBytes(const Flow &flow, double interval) const override {
        return (uint64_t)(GetFlowRate(flow) * interval);
    }

    void UpdateLinkDemand() override {
        fprintf(stderr, "Scanning %zu flows to update demand ...\n", flows.size());
        for (auto &[key, link]: links)
            link.ResetFlowCounts();

        for (const auto &flow: flows) {
            if (!flow.HasStarted(time_now) || flow.IsCompleted())
                continue;

            IncrementLinkFlowCount(flow);
        }
    }

public:
    FatTreeSimulator(HadoopCluster cluster,
                     int over_subscription = 1)
        : ISimulator(cluster)
    {
        printf("[Config] %d:1 Fat-Tree, %" PRIu64 " racks, %" PRIu64 " hosts/rack, %f Gbps link.\n",
                over_subscription, rack_count, hosts_per_rack, link_speed / Gb(1));

        this->host_link_speed = link_speed;
        this->tor_link_speed = link_speed * (double)hosts_per_rack / over_subscription;
        this->over_subscription = over_subscription;

        InitializeLinks();
    }
};

#endif  // FLOW_SIM_FAT_TREE_H
