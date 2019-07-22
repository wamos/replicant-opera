#ifndef FLOW_SIM_TASK_H
#define FLOW_SIM_TASK_H
#include <limits>
#include <string>
#include <vector>
#include <tuple>

struct Task {
    static uint64_t global_task_id;
    uint64_t task_id;
    uint64_t host_id;
    double start_time;
    double end_time;
    std::vector<uint64_t> flows;
    std::string type;

    Task(uint64_t host_id, double start_time, std::string type)
            : host_id(host_id), start_time(start_time), end_time(std::numeric_limits<double>::max()), type(std::move(type))
    {
        task_id = global_task_id++;
    }
};

//uint64_t Task::global_task_id = 0;

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
    std::vector<uint64_t> file_ids;

    ReduceTask() : Task(static_cast<uint64_t>(-1), 0, "reduce") {}

    ReduceTask(uint64_t host_id, double start_time, std::vector<uint64_t> file_ids, std::string sub_task = "")
            : Task(host_id, start_time, "reduce" + sub_task), file_ids(file_ids) {}
};

#endif FLOW_SIM_TASK_H