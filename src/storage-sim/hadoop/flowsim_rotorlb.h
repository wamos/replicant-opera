#ifndef FLOW_SIM_ROTOR_LB_H
#define FLOW_SIM_ROTOR_LB_H

#include "flowsim_sim_interface.h"

#include <numeric>

#define DEFAULT_DUTY_CYCLE 0.9
#define DEFAULT_SLOT_TIME 100e-6

#define ROTOR_LB_TWO_HOP 1
using namespace std;

class RotorNetSimulator : public ISimulator {
private:
    static constexpr double duty_cycle = DEFAULT_DUTY_CYCLE;
    static constexpr double total_slot_time = DEFAULT_SLOT_TIME;
    static constexpr double transmit_slot_time = total_slot_time * duty_cycle;

    const int channel_count;
    const double cycle_time;

#if ROTOR_LB_TWO_HOP
    map<tuple<uint64_t, uint64_t>, vector<uint64_t>> two_hop_rendezvous_racks;
#endif

    inline static uint64_t GetInterTorLinkId(uint64_t src_rack, uint64_t dst_rack) {
        return (src_rack << 32) | (dst_rack & 0xffffffff);
    }

    void InitializeLinks() override {
        double tor_link_speed = link_speed * (double)hosts_per_rack;

        for (uint64_t host_id = 0; host_id < rack_count * hosts_per_rack; host_id++) {
            links.emplace(make_pair(make_tuple(HOST_TOR, host_id), Link(link_speed, channel_count)));
            links.emplace(make_pair(make_tuple(TOR_HOST, host_id), Link(link_speed, channel_count)));
        }

        for (size_t src_rack = 0; src_rack < rack_count; src_rack++){
            for (size_t dst_rack = 0; dst_rack < rack_count; dst_rack++) {
                if (src_rack == dst_rack)
                    continue;
                uint64_t tor_tor_id = GetInterTorLinkId(src_rack, dst_rack);
                links.emplace(make_pair(make_tuple(TOR_TOR, tor_tor_id), Link(tor_link_speed, channel_count)));
            }
        }
    }

    vector<link_id_t> GetLinkIds(const Flow &flow) const override {
        vector<link_id_t> links;

        links.emplace_back(make_tuple(HOST_TOR, flow.src_host));

        const uint64_t mask = 0xffffffff;
        uint64_t src_rack = cluster.GetRackId(flow.src_host) & mask;
        uint64_t dst_rack = cluster.GetRackId(flow.dst_host) & mask;
        if (src_rack != dst_rack) {
            uint64_t tor_tor_id = GetInterTorLinkId(src_rack, dst_rack);
            links.emplace_back(make_tuple(TOR_TOR, tor_tor_id));
        }

        links.emplace_back(make_tuple(TOR_HOST, flow.dst_host));

        return links;
    }

    // Channel is the same as offset in rack,
    // i.e. at channel (time slot) 1, each task sends to rack + 1.
    // no the case, we got a matching matrix now
    int GetFlowChannel(const Flow &flow) const {
        int src_rack = (int)cluster.GetRackId(flow.src_host);
        int dst_rack = (int)cluster.GetRackId(flow.dst_host);
        auto offset = (dst_rack + channel_count - src_rack) % (int)channel_count;
        return offset;
    }

    inline int GetFlowChannel(uint64_t src_rack, uint64_t dst_rack) const {
        return ((int)(dst_rack + (uint64_t) channel_count - src_rack) % channel_count);
    }

    void IncrementLinkFlowCount(const Flow &flow) override {
        int channel = GetFlowChannel(flow);
        for (const auto &link_id: GetLinkIds(flow)) {
            // Host <--> ToR links for intra-rack traffic runs all the time
            if (channel == 0 && get<0>(link_id) != TOR_TOR)
                links.at(link_id).IncrementFlowCount();
            else
                links.at(link_id).IncrementFlowCount(channel);
        }
    }

    // Rate when rotors are down
    double GetFlowRateForDownRotors(const Flow &flow) const {
        uint64_t src_rack = cluster.GetRackId(flow.src_host);
        uint64_t dst_rack = cluster.GetRackId(flow.dst_host);
        if (src_rack == dst_rack) {
            return GetFlowRate(flow);
        } else {
            return 0;
        }
    }

    vector<double> GetRatesPerChannel(const Flow &flow) const {
        uint64_t src_rack = cluster.GetRackId(flow.src_host);
        uint64_t dst_rack = cluster.GetRackId(flow.dst_host);

        vector<double> rates(channel_count);            // flow rate at each channel.

        if (src_rack == dst_rack) {
            for (int channel = 0; channel < channel_count; channel++)
                rates[channel] = GetFlowRate(flow, channel);
        } else {
            fill(rates.begin(), rates.end(), (double)0);
            rates[GetFlowChannel(flow)] = GetFlowRate(flow, GetFlowChannel(flow)); // One-hop rate

#if ROTOR_LB_TWO_HOP
            for (auto mid_rack: two_hop_rendezvous_racks.at(make_tuple(src_rack, dst_rack))) {
                vector<double> link_rates;

                // Hop 1 - uplink to rendezvous rack
                int uplink_channel = GetFlowChannel(src_rack, mid_rack);
                auto uplink1_id = make_tuple(HOST_TOR, flow.src_host);
                auto uplink2_id = make_tuple(TOR_TOR, GetInterTorLinkId(src_rack, mid_rack));
                link_rates.push_back(links.at(uplink1_id).GetRatePerFlow(uplink_channel));
                link_rates.push_back(links.at(uplink2_id).GetRatePerFlow(uplink_channel));

                // Hop 2 - downlink from rendezvous rack
                int downlink_channel = GetFlowChannel(mid_rack, dst_rack);
                auto downlink1_id = make_tuple(TOR_TOR, GetInterTorLinkId(mid_rack, dst_rack));
                auto downlink2_id = make_tuple(TOR_HOST, flow.dst_host);
                link_rates.push_back(links.at(downlink1_id).GetRatePerFlow(downlink_channel));
                link_rates.push_back(links.at(downlink2_id).GetRatePerFlow(downlink_channel));

                auto rate = *min_element(link_rates.begin(), link_rates.end());
                rates[downlink_channel] += rate;    // Count rate against the completing channel
            }
#endif
        }

        return rates;
    }

    vector<uint64_t> GetBytesPerChannel(const Flow &flow, vector <double> &rates) const {
        uint64_t src_rack = cluster.GetRackId(flow.src_host);
        uint64_t dst_rack = cluster.GetRackId(flow.dst_host);

        vector<uint64_t> bytes_per_slot(channel_count); // # of bytes this flow can transmit per time slot.

        if (src_rack == dst_rack) {
            double rate_down = GetFlowRateForDownRotors(flow);
            for (int channel = 0; channel < channel_count; channel++) {
                auto rate = GetFlowRate(flow, channel);
                bytes_per_slot[channel] = (uint64_t) (rate * transmit_slot_time
                                                      + rate_down * (total_slot_time - transmit_slot_time));
            }
        } else {
            for (int channel = 0; channel < channel_count; channel++)
                bytes_per_slot[channel] = (uint64_t) (rates[channel] * transmit_slot_time);
        }

        return bytes_per_slot;
    }

    // Assume simple matching matrix, where time slot t means
    //  each host h is connected to (h + t) % host_count.
    double GetFlowRemainingTime(const Flow &flow) const override {
        auto rates = GetRatesPerChannel(flow);
        auto bytes_per_slot = GetBytesPerChannel(flow, rates);
        double rate_down = GetFlowRateForDownRotors(flow);

        uint64_t total_bytes = flow.GetRemainingBytes();

        // Get to end of current slot
        double time_in_cycle = fmod(time_now, cycle_time);
        double time_in_slot = fmod(time_in_cycle, total_slot_time);
        int channel_base = (int)ceil(time_in_cycle / total_slot_time);

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

        // Now we are in rotor down time zone
        double whole_slot_remaining_time = total_slot_time - time_in_slot;
        if (rate_down != 0.) {
            auto partial_bytes = (uint64_t) (rate_down * whole_slot_remaining_time);
            if (total_bytes <= partial_bytes)
                return start_partial_slot_time + (double) total_bytes / rate_down;
            else
                total_bytes -= partial_bytes;
        }

        // Start of new slot
        start_partial_slot_time += whole_slot_remaining_time;
        channel_base += 1;

        // Go through whole cycles
        uint64_t bytes_per_cycle = accumulate(bytes_per_slot.begin(), bytes_per_slot.end(), (uint64_t)0);
        uint64_t whole_cycles = total_bytes / bytes_per_cycle;
        double whole_cycles_time = (double)whole_cycles * cycle_time;

        // Run through remaining slots
        uint64_t remaining_bytes = total_bytes % bytes_per_cycle;
        double remaining_time = 0;
        int channel_index = 0;
        while (remaining_bytes > 0 && channel_index < channel_count) {
            auto channel = static_cast<size_t>((channel_base + channel_index) % channel_count);
            if (remaining_bytes < bytes_per_slot[channel]) {
                auto rate = rates[channel];
                auto transmit_bytes = (uint64_t)(rate * transmit_slot_time);
                if (remaining_bytes < transmit_bytes) {
                    remaining_time += (double)remaining_bytes / rate;
                } else {
                    remaining_time += transmit_slot_time;
                    remaining_bytes -= transmit_bytes;
                    remaining_time += (double)remaining_bytes / rate_down;
                }

                break;
            }

            remaining_time += total_slot_time;
            remaining_bytes -= bytes_per_slot[channel];
            channel_index++;
        }

        if (channel_index >= channel_count)
            throw runtime_error("Out of bound on channel index.\n");

        return start_partial_slot_time + whole_cycles_time + remaining_time;
    }

    uint64_t GetTransmittedBytes(const Flow &flow, double interval) const override {
        auto rates = GetRatesPerChannel(flow);
        auto bytes_per_slot = GetBytesPerChannel(flow, rates);
        double rate_down = GetFlowRateForDownRotors(flow);

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
        if (interval <= whole_slot_remaining_time) {
            return bytes_transmitted + (uint64_t) (rate_down * whole_slot_remaining_time);
        } else {
            bytes_transmitted += (uint64_t) (rate_down * whole_slot_remaining_time);
            interval -= whole_slot_remaining_time;
        }

        // Start of new slot
        channel_base += 1;

        // Go through whole cycles
        auto bytes_per_cycle = accumulate(bytes_per_slot.begin(), bytes_per_slot.end(), (uint64_t)0);
        auto whole_cycles = (uint64_t)ceil(interval / cycle_time);
        bytes_transmitted += whole_cycles * bytes_per_cycle;

        // Run through remaining time
        double remaining_time = fmod(interval, cycle_time);
        auto whole_slots_left = (int)(remaining_time / total_slot_time);
        auto slot_time_left = fmod(remaining_time, total_slot_time);
        bytes_transmitted += accumulate(bytes_per_slot.begin(), bytes_per_slot.begin() + whole_slots_left, (uint64_t)0);
        auto channel = (channel_base + whole_slots_left) % channel_count;
        if (slot_time_left <= transmit_slot_time) {
            bytes_transmitted += (uint64_t) (slot_time_left * rates[channel]);
        } else {
            bytes_transmitted += (uint64_t) (transmit_slot_time * rates[channel]);
            bytes_transmitted += (uint64_t) (rate_down * (transmit_slot_time - slot_time_left));
        }

        return bytes_transmitted;
    }

    void UpdateLinkDemand() override {
        fprintf(stderr, "Scanning %zu flows to update demand ...\n", flows.size());
        for (auto &[key, link]: links)
            link.ResetFlowCounts();
#if ROTOR_LB_TWO_HOP
        two_hop_rendezvous_racks.clear();
#endif

        // Whether there is one-hop traffic between racks.
#if ROTOR_LB_TWO_HOP
        map<tuple<uint64_t, uint64_t>, bool> has_direct_traffic;
#endif
        for (const auto &flow: flows) {
            if (!flow.HasStarted(time_now) || flow.IsCompleted())
                continue;

            IncrementLinkFlowCount(flow);
#if ROTOR_LB_TWO_HOP
            has_direct_traffic[make_tuple(GetRackId(flow.src_host), GetRackId(flow.dst_host))] = true;
#endif
        }

#if ROTOR_LB_TWO_HOP
        // Add two-hop paths if not interfering with one-hop traffic
        for (const auto &flow: flows) {
            if (!flow.HasStarted(time_now) || flow.IsCompleted())
                continue;

            uint64_t src_rack = GetRackId(flow.src_host);
            uint64_t dst_rack = GetRackId(flow.dst_host);
            if (src_rack == dst_rack)
                continue;

            auto rack_key = make_tuple(src_rack, dst_rack);
            if (two_hop_rendezvous_racks.find(rack_key) == two_hop_rendezvous_racks.end()) {
                vector<uint64_t> mid_racks;
                for (uint64_t mid_rack = 0; mid_rack < cluster.rack_count; mid_rack++) {
                    if (mid_rack == src_rack || mid_rack == dst_rack)
                        continue;

                    if (has_direct_traffic[make_tuple(src_rack, mid_rack)] ||
                        has_direct_traffic[make_tuple(mid_rack, dst_rack)])
                        continue;

                    mid_racks.push_back(mid_rack);
                }

                two_hop_rendezvous_racks[rack_key] = mid_racks;
            }

            for (auto mid_rack: two_hop_rendezvous_racks[rack_key]) {
                // Hop 1 - uplink to rendezvous rack
                int uplink_channel = GetFlowChannel(src_rack, mid_rack);
                auto uplink1 = make_tuple(HOST_TOR, flow.src_host);
                auto uplink2 = make_tuple(TOR_TOR, GetInterTorLinkId(src_rack, mid_rack));
                links.at(uplink1).IncrementFlowCount(uplink_channel);
                links.at(uplink2).IncrementFlowCount(uplink_channel);

                // Hop 2 - downlink from rendezvous rack
                int downlink_channel = GetFlowChannel(mid_rack, dst_rack);
                auto downlink1 = make_tuple(TOR_TOR, GetInterTorLinkId(mid_rack, dst_rack));
                auto downlink2 = make_tuple(TOR_HOST, flow.dst_host);
                links.at(downlink1).IncrementFlowCount(downlink_channel);
                links.at(downlink2).IncrementFlowCount(downlink_channel);
            }
        }
#endif
    }

public:
    RotorNetSimulator(SimpleCluster cluster) : ISimulator(cluster),
            channel_count((int)rack_count),
            cycle_time(total_slot_time * channel_count) {
        printf("[Config] RotorNet, %" PRIu64 " racks, %" PRIu64 " hosts/rack, %f Gbps link.\n",
                rack_count, hosts_per_rack, link_speed / Gb(1));

        InitializeLinks();
    }
};

#endif  // FLOW_SIM_ROTOR_LB_H
