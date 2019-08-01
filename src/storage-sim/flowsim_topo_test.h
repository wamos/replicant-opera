#ifndef FLOW_SIM_TEST_TOPO_H
#define FLOW_SIM_TEST_TOPO_H

#include <vector>
#include "simple_cluster.h"
#include "flowsim_sim_interface.h"

class TestTopologySimulator : public ISimulator {
private:
    void InitializeLinks() override;
    std::vector<link_id_t> GetLinkIds(const Flow &flow) const override;
    void IncrementLinkFlowCount(const Flow &flow) override;
    double GetFlowRemainingTime(const Flow &flow) const override;
    uint64_t GetTransmittedBytes(const Flow &flow, double interval) const override;
    void UpdateLinkDemand() override;
public:
    TestTopologySimulator();

    TestTopologySimulator(SimpleCluster cluster)
        : ISimulator(cluster){
        printf("[Config] Test-Topology, %" PRIu64 " racks, %" PRIu64 " hosts/rack, %f Gbps link.\n",
              rack_count, hosts_per_rack, link_speed / Gb(1));
        //host_link_speed = link_speed;
        InitializeLinks();
    }
};


#endif //FLOW_SIM_TEST_TOPO_H