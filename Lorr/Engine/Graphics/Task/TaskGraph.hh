#pragma once

#include "Task.hh"

#include "Engine/Util/LegitProfiler.hh"

namespace lr::graphics {
struct TaskExecuteInfo {
    u32 frame_index = 0;
    ls::span<SemaphoreSubmitInfo> wait_semas = {};
    ls::span<SemaphoreSubmitInfo> signal_semas = {};
};

struct TaskGraphInfo {
    Device *device = nullptr;
};

struct TaskGraph {
    std::vector<std::unique_ptr<Task>> tasks = {};
    std::vector<TaskSubmit> submits = {};
    std::vector<TaskBarrier> barriers = {};
    std::vector<TaskImage> images = {};
    std::vector<TaskBuffer> buffers = {};

    // Profilers
    ls::static_vector<TimestampQueryPool, Limits::FrameCount> task_query_pools = {};
    std::vector<legit::ProfilerTask> task_gpu_profiler_tasks = {};
    legit::ProfilerGraph task_gpu_profiler_graph = { 400 };

    Device *device = nullptr;

    bool init(this TaskGraph &, const TaskGraphInfo &info);

    TaskImageID add_image(this TaskGraph &, const TaskPersistentImageInfo &info);
    void set_image(this TaskGraph &, TaskImageID task_image_id, const TaskPersistentImageInfo &info);

    u32 schedule_task(this TaskGraph &, Task *task, TaskSubmit &submit);

    // Recording
    template<typename TaskT>
    TaskID add_task(const TaskT &task_info);
    void present(this TaskGraph &, TaskImageID task_image_id);

    // Debug tools
    std::string generate_graphviz(this TaskGraph &);
    void draw_profiler_ui(this TaskGraph &);

    // Rendering
    void execute(this TaskGraph &, const TaskExecuteInfo &info);

private:
    TaskID add_task(this TaskGraph &, std::unique_ptr<Task> &&task);
    bool prepare_task(this TaskGraph &, Task &task);
    TaskSubmit &new_submit(this TaskGraph &, CommandType type);

    TaskImage &task_image_at(this auto &self, TaskImageID id) { return self.images[static_cast<usize>(id)]; }
    TaskBuffer &task_buffer_at(this auto &self, TaskBufferID id) { return self.buffers[static_cast<usize>(id)]; }
};

template<typename TaskT>
TaskID TaskGraph::add_task(const TaskT &task_info)
{
    ZoneScoped;

    std::unique_ptr<Task> task = std::make_unique<TaskWrapper<TaskT>>(task_info);
    return add_task(std::move(task));
}

}  // namespace lr::graphics
