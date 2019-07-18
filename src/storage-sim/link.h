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


