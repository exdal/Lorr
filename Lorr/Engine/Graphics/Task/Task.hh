#pragma once

#include "Engine/Graphics/Common.hh"
#include "Engine/Graphics/CommandList.hh"

#include "TaskResource.hh"

namespace lr {
struct TaskPrepareInfo {
    Device *device = nullptr;
    TaskPipelineInfo pipeline_info = {};
};

struct TaskContext;
struct Task {
    virtual ~Task() = default;
    virtual bool prepare(TaskPrepareInfo &info) = 0;
    virtual void execute(TaskContext &tc) = 0;

    PipelineID pipeline_id = PipelineID::Invalid;
    PipelineLayoutID pipeline_layout_id = PipelineLayoutID::None;
    ls::span<TaskUse> task_uses = {};
    std::string_view name = {};
    f64 start_ts = 0.0f;
    f64 end_ts = 0.0f;
    Extent2D render_extent = {};

    f64 execution_time() { return (end_ts - start_ts) / 1000000.f; }
};

template<typename TaskT>
concept TaskConcept = requires {
    TaskT{}.uses;
    TaskT{}.name;
} and requires(TaskContext &tc) { TaskT{}.execute(tc); };

template<typename TaskT>
concept TaskHasPushConstants = requires { typename TaskT::PushConstants; };

template<TaskConcept TaskT>
struct TaskWrapper : Task {
    TaskWrapper(const TaskT &task_)
        : task(task_) {
        if constexpr (TaskHasPushConstants<TaskT>) {
            pipeline_layout_id = static_cast<PipelineLayoutID>(sizeof(typename TaskT::PushConstants) / sizeof(u32));
        }

        task_uses = { reinterpret_cast<TaskUse *>(&task.uses), TASK_USE_COUNT };
        name = task.name;
    }

    bool prepare(TaskPrepareInfo &info) override {
        constexpr bool has_prepare = requires { TaskT{}.prepare(info); };
        if constexpr (has_prepare) {
            return task.prepare(info);
        }

        return false;
    }

    void execute(TaskContext &tc) override { task.execute(tc); }

    TaskT task = {};
    constexpr static usize TASK_USE_COUNT = sizeof(typename TaskT::Uses) / sizeof(TaskUse);
};

struct TaskBarrier {
    ImageLayout src_layout = ImageLayout::Undefined;
    ImageLayout dst_layout = ImageLayout::Undefined;
    PipelineAccessImpl src_access = PipelineAccess::None;
    PipelineAccessImpl dst_access = PipelineAccess::None;
    TaskImageID image_id = TaskImageID::Invalid;

    bool is_image() { return image_id != TaskImageID::Invalid; }
};

struct TaskBatch {
    PipelineAccessImpl execution_access = PipelineAccess::None;
    std::vector<u32> barrier_indices = {};
    std::vector<TaskID> tasks = {};
    f64 start_ts = 0.0f;
    f64 end_ts = 0.0f;

    f64 execution_time() { return (end_ts - start_ts) / 1000000.f; }
};

struct TaskSubmit {
    CommandType type = CommandType::Graphics;

    std::vector<TaskBatch> batches = {};
    std::array<TimestampQueryPool, Limits::FrameCount> query_pools = {};

    // Additional pipeline barriers that will be executed after
    std::vector<u32> additional_signal_barrier_indices = {};

    auto &frame_query_pool(u32 frame_index) { return query_pools[frame_index]; }
};

}  // namespace lr
