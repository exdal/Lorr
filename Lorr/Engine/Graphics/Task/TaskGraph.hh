#pragma once

#include "Task.hh"

#include "Engine/Util/LegitProfiler.hh"

namespace lr::graphics {
struct TaskExecuteInfo {
    u32 image_index = 0;
    std::span<SemaphoreSubmitInfo> wait_semas = {};
    std::span<SemaphoreSubmitInfo> signal_semas = {};
};

struct TaskGraphInfo {
    Device *device = nullptr;
};

struct TaskGraph {
    std::vector<std::unique_ptr<Task>> m_tasks = {};
    std::vector<TaskSubmit> m_submits = {};
    std::vector<TaskBarrier> m_barriers = {};
    std::vector<TaskImage> m_images = {};
    std::vector<TaskBuffer> m_buffers = {};

    // Profilers
    std::array<TimestampQueryPool, Limits::FrameCount> m_task_query_pools = {};
    std::vector<legit::ProfilerTask> m_task_gpu_profiler_tasks = {};
    legit::ProfilerGraph m_task_gpu_profiler_graph = { 400 };

    Device *m_device = nullptr;

    bool init(const TaskGraphInfo &info);

    TaskImageID add_image(const TaskPersistentImageInfo &info);
    void set_image(TaskImageID task_image_id, const TaskPersistentImageInfo &info);

    u32 schedule_task(Task *task, TaskSubmit &submit);

    // Recording
    template<typename TaskT>
    TaskID add_task(const TaskT &task_info);
    TaskID add_task(std::unique_ptr<Task> &&task);
    void present(TaskImageID task_image_id);

    // Debug tools
    std::string generate_graphviz();
    void draw_profiler_ui();

    // Rendering
    void execute(const TaskExecuteInfo &info);

private:
    bool prepare_task(Task &task);
    TaskSubmit &new_submit(CommandType type);
};

template<typename TaskT>
TaskID TaskGraph::add_task(const TaskT &task_info)
{
    ZoneScoped;

    std::unique_ptr<Task> task = std::make_unique<TaskWrapper<TaskT>>(task_info);
    return add_task(std::move(task));
}

}  // namespace lr::graphics
