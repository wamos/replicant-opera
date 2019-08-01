#ifndef FLOW_SIM_SINGLE_ROTOR_H
#define FLOW_SIM_SINGLE_ROTOR_H

#include <vector>
#include "simple_cluster.h"
#include "flowsim_sim_interface.h"

class SingleLayerRotorSimulator : public ISimulator {
private:

    static const double transmit_slot_time = DEFAULT_SLOT_TIME * DEFAULT_DUTY_CYCLE;
    const int channel_count;
    const double cycle_time;

    //inherit these from ISimulator
    void InitializeLinks() override;
    std::vector<link_id_t> GetLinkIds(const Flow &flow) const override;
    void IncrementLinkFlowCount(const Flow &flow) override;
    double GetFlowRemainingTime(const Flow &flow) const override;
    uint64_t GetTransmittedBytes(const Flow &flow, double interval) const override;
    void UpdateLinkDemand() override;

    // our own rotor-specific methods as follow:
    void InitializeMatchingMatrix();
    int GetFlowChannel(const Flow &flow) const;
    int GetFlowChannel(uint64_t src, uint64_t dst) const;
    double GetFlowRateForDownRotors(const Flow &flow) const;
    std::vector<double> GetRatesPerChannel(const Flow &flow) const;
    std::vector<uint64_t> GetBytesPerChannel(const Flow &flow, std::vector <double> &rates) const;

public:
    SingleLayerRotorSimulator();

    SingleLayerRotorSimulator(SimpleCluster cluster)
        : ISimulator(cluster),
        channel_count(ROTOR_CHANNEL),
        cycle_time(DEFAULT_SLOT_TIME*ROTOR_CHANNEL)
        {
        printf("[Config] Test-Topology, %" PRIu64 " racks, %" PRIu64 " hosts/rack, %f Gbps link.\n",
              rack_count, hosts_per_rack, link_speed / Gb(1));
        //host_link_speed = link_speed;
        InitializeLinks();
    }
};


#endif //FLOW_SIM_SINGLE_ROTOR_H