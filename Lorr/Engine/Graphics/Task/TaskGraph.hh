#pragma once

#include "Task.hh"
#include "TaskResource.hh"

#include "Engine/Util/LegitProfiler.hh"

namespace lr {
struct TaskExecuteInfo {
    u32 frame_index = 0;
    void *execution_data = nullptr;  // This is custom user data
    ls::span<Semaphore> wait_semas = {};
    ls::span<Semaphore> signal_semas = {};
};

struct TaskGraph {
    constexpr static usize MAX_PERMUTATIONS = 32;

    std::vector<std::unique_ptr<Task>> tasks = {};
    std::vector<TaskSubmit> submits = {};
    std::vector<TaskBatch> batches = {};
    std::vector<TaskBarrier> barriers = {};

    std::vector<TaskImage> images = {};
    std::vector<TaskBufferInfo> buffers = {};

    // Profilers
    ls::static_vector<QueryPool, Device::Limits::FrameCount> task_query_pools = {};
    std::vector<legit::ProfilerTask> task_gpu_profiler_tasks = {};
    legit::ProfilerGraph task_gpu_profiler_graph = { 400 };

    Device device = {};

    bool init(this TaskGraph &, Device device_);

    TaskImageID add_image(this TaskGraph &, const TaskImageInfo &info);
    void set_image(this TaskGraph &, TaskImageID task_image_id, const TaskImageInfo &info);
    TaskBufferID add_buffer(this TaskGraph &, const TaskBufferInfo &info);

    // Recording
    template<typename TaskT = InlineTask>
    TaskID add_task(this TaskGraph &, const TaskT &task_info);
    void present(this TaskGraph &, TaskImageID task_image_id);

    // Debug tools
    void draw_profiler_ui(this TaskGraph &);

    // Rendering
    void execute(this TaskGraph &, const TaskExecuteInfo &info);

    template<TaskConcept T>
    usize get_task_color_attachment_count();

    Task &task_at(this TaskGraph &self, TaskID id) { return *self.tasks[static_cast<usize>(id)]; }
    TaskImage &task_image_at(this TaskGraph &self, TaskImageID id) { return self.images[static_cast<usize>(id)]; }
    TaskBufferInfo &task_buffer_at(this TaskGraph &self, TaskBufferID id) { return self.buffers[static_cast<usize>(id)]; }

private:
    TaskID add_task(this TaskGraph &, std::unique_ptr<Task> &&task);
    TaskSubmit &new_submit(this TaskGraph &, vk::CommandType type);
};

template<typename TaskT>
TaskID TaskGraph::add_task(this TaskGraph &self, const TaskT &task_info) {
    ZoneScoped;

    std::unique_ptr<Task> task = std::make_unique<TaskWrapper<TaskT>>(task_info);
    return self.add_task(std::move(task));
}

template<TaskConcept TaskT>
usize TaskGraph::get_task_color_attachment_count() {
    usize count = 0;

    constexpr static usize TASK_USE_COUNT = sizeof(typename TaskT::Uses) / sizeof(TaskUse);
    typename TaskT::Uses uses = {};
    ls::span task_uses = { reinterpret_cast<TaskUse *>(&uses), TASK_USE_COUNT };

    for_each_image_use(task_uses, [&](TaskImageID, const vk::PipelineAccessImpl &, vk::ImageLayout layout) {
        switch (layout) {
            case vk::ImageLayout::ColorAttachment:
                count++;
                break;
            default:;
        }
    });

    return count;
}

struct TaskContext {
    Device &device;
    TaskGraph &task_graph;
    Task &task;
    CommandList &cmd_list;

private:
    void *execution_data;

public:
    vk::Extent2D render_extent = {};
    ls::option<PipelineID> pipeline_id = ls::nullopt;
    ls::option<PipelineLayoutID> pipeline_layout_id = ls::nullopt;

    TaskContext(TaskGraph &task_graph_, Task &task_, CommandList &cmd_list_, void *execution_data_);

    vk::RenderingAttachmentInfo as_color_attachment(
        this TaskContext &, TaskUse &use, std::optional<vk::ColorClearValue> clear_val = std::nullopt);
    vk::RenderingAttachmentInfo as_depth_attachment(
        this TaskContext &, TaskUse &use, std::optional<vk::DepthClearValue> clear_val = std::nullopt);
    TaskImage &task_image_data(this TaskContext &, TaskUse &use);
    TaskImage &task_image_data(this TaskContext &, TaskImageID task_image_id);
    vk::Viewport pass_viewport(this TaskContext &);
    vk::Rect2D pass_rect(this TaskContext &);

    void set_pipeline(this TaskContext &, PipelineID pipeline_id);

    template<typename T>
    void set_push_constants(this TaskContext &self, T &v) {
        ZoneScoped;

        self.cmd_list.set_push_constants(*self.pipeline_layout_id, &v, sizeof(T), 0);
    }

    template<typename T = void>
    T &exec_data_as() {
        return *static_cast<T *>(execution_data);
    }
};

}  // namespace lr
