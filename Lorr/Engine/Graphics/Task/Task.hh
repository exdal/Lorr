#pragma once

#include "TaskResource.hh"

namespace lr {
enum class TaskID : u32 { Invalid = ~0_u32 };

struct TaskContext;
struct Task {
    virtual ~Task() = default;
    virtual void execute(TaskContext &tc) = 0;

    ls::span<TaskUse> task_uses = {};
    std::string name = {};
    f64 start_ts = 0.0f;
    f64 end_ts = 0.0f;

    f64 execution_time() { return (end_ts - start_ts) / 1000000.f; }
};

struct InlineTask {
    std::string name = {};
    std::vector<TaskUse> uses = {};
    std::function<void(TaskContext &)> execute_cb = {};

    void execute(TaskContext &tc) { execute_cb(tc); }
};

template<typename TaskT>
concept TaskConcept = requires() {
    TaskT{}.uses;
    TaskT{}.name;
} and requires(TaskContext &tc) { TaskT{}.execute(tc); };

template<typename TaskT>
concept TaskHasPushConstants = requires { typename TaskT::PushConstants; };

template<TaskConcept TaskT>
struct TaskWrapper : Task {
    TaskWrapper(const TaskT &task_)
        : task(task_) {
        task_uses = { reinterpret_cast<TaskUse *>(&task.uses), TASK_USE_COUNT };
        name = task.name;
    }

    void execute(TaskContext &tc) override { task.execute(tc); }

    TaskT task = {};
    constexpr static usize TASK_USE_COUNT = sizeof(decltype(task.uses)) / sizeof(TaskUse);
};

struct TaskBarrier {
    vk::ImageLayout src_layout = vk::ImageLayout::Undefined;
    vk::ImageLayout dst_layout = vk::ImageLayout::Undefined;
    vk::PipelineAccessImpl src_access = vk::PipelineAccess::None;
    vk::PipelineAccessImpl dst_access = vk::PipelineAccess::None;
    TaskImageID image_id = TaskImageID::Invalid;

    bool is_image() { return image_id != TaskImageID::Invalid; }
};

struct TaskBatch {
    vk::PipelineAccessImpl execution_access = vk::PipelineAccess::None;
    std::vector<u32> barrier_indices = {};
    std::vector<TaskID> tasks = {};
    f64 start_ts = 0.0f;
    f64 end_ts = 0.0f;

    f64 execution_time() { return (end_ts - start_ts) / 1000000.f; }
};

struct TaskSubmit {
    vk::CommandType type = vk::CommandType::Graphics;
    std::array<QueryPool, Device::Limits::FrameCount> query_pools = {};
    std::vector<usize> batch_indices = {};
    // Additional pipeline barriers that will be executed after
    std::vector<u32> additional_signal_barrier_indices = {};
};

}  // namespace lr
