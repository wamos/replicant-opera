#ifndef FLOW_SIM_TASK_H
#define FLOW_SIM_TASK_H
#include <limits>
#include <string>
#include <vector>
#include <tuple>

static uint64_t global_task_id = 0;
struct Task {
    //static uint64_t global_task_id;
    uint64_t task_id;
    uint64_t host_id;
    double start_time;
    double end_time;
    std::vector<uint64_t> flows;
    std::string tag;
    bool isCompleted;

    Task(uint64_t host_id, double start_time, std::string tag)
        : host_id(host_id), 
        start_time(start_time), 
        end_time(std::numeric_limits<double>::max()), 
        tag(std::move(tag)),
        isCompleted(false){ // assume no dep task, so its upstream task is always completed
        task_id = global_task_id++;
    }
};

struct BlockReadTask : Task {
    uint64_t file_id;
    uint64_t dst_id;
    std::vector<uint64_t> block_ids;
    Task* dep_task;
    bool isCompleted;

    BlockReadTask() : Task(static_cast<uint64_t>(-1), 0, "block-read"),
            file_id(static_cast<uint64_t>(-1)){}

    BlockReadTask(uint64_t host_id, uint64_t dst_id,double start_time, uint64_t file_id)
            : Task(host_id, start_time, "block-read"), 
            file_id(file_id){}

    BlockReadTask(uint64_t host_id, uint64_t dst_id, double start_time, uint64_t file_id, Task* task)
            : Task(host_id, start_time, "block-read"), 
            dst_id(dst_id),
            dep_task(task),
            isCompleted(false){}
    
    bool setDependency(Task* task){
        dep_task = task;
        task->isCompleted = false;
    }
};

struct RWTask: Task{
    uint64_t file_id;
    uint64_t dst_id;
    uint64_t block_id;
    Task* dep_task;

    RWTask() : Task(static_cast<uint64_t>(-1), 0, "seq_read"),
            file_id(static_cast<uint64_t>(-1)),
            block_id(static_cast<uint64_t>(-1)) {}

    RWTask(uint64_t host_id, uint64_t dst_id,double start_time, uint64_t file_id, std::string tag)
            : Task(host_id, start_time, tag), 
            dst_id(dst_id),
            file_id(file_id),
            dep_task(nullptr)
            {}

    RWTask(uint64_t host_id, uint64_t dst_id, double start_time, uint64_t file_id, uint64_t block_id, std::string tag)
            :Task(host_id, start_time, tag), 
            file_id(file_id), 
            block_id(block_id), 
            dst_id(dst_id),
            dep_task(nullptr){} 

    void setDependency(Task* task){
        dep_task = task;
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
    std::vector<uint64_t> file_ids;

    ReduceTask() : Task(static_cast<uint64_t>(-1), 0, "reduce") {}

    ReduceTask(uint64_t host_id, double start_time, std::vector<uint64_t> file_ids, std::string sub_task = "")
            : Task(host_id, start_time, "reduce" + sub_task), file_ids(file_ids) {}
};

#endif //FLOW_SIM_TASK_H