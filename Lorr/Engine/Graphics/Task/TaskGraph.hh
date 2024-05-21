#pragma once

#include "Task.hh"

#include "Engine/Graphics/CommandList.hh"

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
    bool init(const TaskGraphInfo &info);

    TaskImageID add_image(const TaskPersistentImageInfo &info);
    void set_image(TaskImageID task_image_id, const TaskPersistentImageInfo &info);

    u32 schedule_task(Task *task, TaskSubmit &submit);

    template<typename TaskT>
    TaskID add_task(const TaskT &task_info);
    TaskID add_task(std::unique_ptr<Task> &&task);
    void present(TaskImageID task_image_id);
    std::string generate_graphviz();

    void execute(const TaskExecuteInfo &info);

    std::vector<std::unique_ptr<Task>> m_tasks = {};
    std::vector<TaskSubmit> m_submits = {};
    std::vector<TaskBarrier> m_barriers = {};
    std::vector<TaskImage> m_images = {};
    std::vector<TaskBuffer> m_buffers = {};

    std::array<std::array<CommandAllocator, 3>, Limits::FrameCount> m_command_allocators = {};
    // TODO: Multiple submits
    std::array<TimestampQueryPool, Limits::FrameCount> m_timestamp_query_pools = {};

    Device *m_device = nullptr;
};

template<typename TaskT>
TaskID TaskGraph::add_task(const TaskT &task_info)
{
    ZoneScoped;

    std::unique_ptr<Task> task = std::make_unique<TaskWrapper<TaskT>>(task_info);
    return add_task(std::move(task));
}

}  // namespace lr::graphics
