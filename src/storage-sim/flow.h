#ifndef FLOW_SIM_FLOW_H
#define FLOW_SIM_FLOW_H

#include <limits>
#include "task.h"
#include "flowsim_config_macro.h"

static uint64_t global_flow_id = 0;
struct Flow {
    int src_channel;
    int dst_channel;
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
        this->time_end = std::numeric_limits<double>::max();
        this->total_bytes = size;
        this->completed_bytes = 0;
//        this->current_rate = 0;
        this->task = task;

        this->src_host = src_host;
        this->dst_host = dst_host;
    }

    void setChannels(int src_ch, int dst_ch){
        src_channel = src_ch;
        dst_channel = dst_ch;
    }

    void setChannels(std::tuple<int, int> channels){
        src_channel = std::get<0>(channels);
        dst_channel = std::get<1>(channels);
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

#endif  //FLOW_SIM_FLOW_H
