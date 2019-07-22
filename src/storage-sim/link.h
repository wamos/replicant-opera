#ifndef FLOW_SIM_LINK_H
#define FLOW_SIM_LINK_H
#include <tuple>
#include <vector>
#include "flowsim_config_macro.h"

struct Link {
    double capacity;
    int flow_count; // # of flows always transmitting
    std::vector<int> flow_counts; // # of flows in TDMA channels

    Link(double capacity, int channel_count = 0)
        : capacity(capacity), flow_count(0), flow_counts(std::vector<int>(static_cast<unsigned long>(channel_count))) {}

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

#endif //FLOW_SIM_LINK_H
