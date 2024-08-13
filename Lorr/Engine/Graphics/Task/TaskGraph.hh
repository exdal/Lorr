#pragma once

#include "Task.hh"
#include "TaskResource.hh"

#include "Engine/Graphics/Device.hh"
#include "Engine/Util/LegitProfiler.hh"

namespace lr {
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
    std::vector<TaskBufferInfo> buffers = {};

    // Profilers
    ls::static_vector<TimestampQueryPool, Limits::FrameCount> task_query_pools = {};
    std::vector<legit::ProfilerTask> task_gpu_profiler_tasks = {};
    legit::ProfilerGraph task_gpu_profiler_graph = { 400 };

    Device *device = nullptr;

    bool init(this TaskGraph &, const TaskGraphInfo &info);

    TaskImageID add_image(this TaskGraph &, const TaskPersistentImageInfo &info);
    void set_image(this TaskGraph &, TaskImageID task_image_id, const TaskPersistentImageInfo &info);
    TaskBufferID add_buffer(this TaskGraph &, const TaskBufferInfo &info);

    u32 schedule_task(this TaskGraph &, Task *task, TaskSubmit &submit);
    // Used to update every task infos, ie, render_extent
    void update_task_infos(this TaskGraph &);

    // Recording
    template<typename TaskT>
    TaskID add_task(const TaskT &task_info);
    void present(this TaskGraph &, TaskImageID task_image_id);

    // Debug tools
    std::string generate_graphviz(this TaskGraph &);
    void draw_profiler_ui(this TaskGraph &);

    // Rendering
    void execute(this TaskGraph &, const TaskExecuteInfo &info);

    TaskImage &task_image_at(this auto &self, TaskImageID id) { return self.images[static_cast<usize>(id)]; }
    TaskBufferInfo &task_buffer_at(this auto &self, TaskBufferID id) { return self.buffers[static_cast<usize>(id)]; }

private:
    TaskID add_task(this TaskGraph &, std::unique_ptr<Task> &&task);
    bool prepare_task(this TaskGraph &, Task &task);
    TaskSubmit &new_submit(this TaskGraph &, CommandType type);
};

template<typename TaskT>
TaskID TaskGraph::add_task(const TaskT &task_info) {
    ZoneScoped;

    std::unique_ptr<Task> task = std::make_unique<TaskWrapper<TaskT>>(task_info);
    return add_task(std::move(task));
}

struct TaskContext {
    TaskContext(Device &device_, TaskGraph &task_graph_, Task &task_, CommandList &cmd_list_, CommandList &copy_cmd_list_, usize frame_index_)
        : device(device_),
          task_graph(task_graph_),
          task(task_),
          cmd_list(cmd_list_),
          copy_cmd_list(copy_cmd_list_),
          frame_index(frame_index_) {}

    template<typename T>
    RenderingAttachmentInfo as_color_attachment(this TaskContext &self, T &use, std::optional<ColorClearValue> clear_val = std::nullopt) {
        ZoneScoped;

        RenderingAttachmentInfo info = {
            .image_view_id = self.task_image_data(use).image_view_id,
            .image_layout = ImageLayout::ColorAttachment,
            .load_op = clear_val ? AttachmentLoadOp::Clear : AttachmentLoadOp::Load,
            .store_op = use.access.access & MemoryAccess::Write ? AttachmentStoreOp::Store : AttachmentStoreOp::None,
            .clear_value = { clear_val.value_or(ColorClearValue{}) },
        };

        return info;
    }

    template<typename T>
    RenderingAttachmentInfo as_depth_attachment(this TaskContext &self, T &use, std::optional<DepthClearValue> clear_val = std::nullopt) {
        ZoneScoped;

        RenderingAttachmentInfo info = {
            .image_view_id = self.task_image_data(use).image_view_id,
            .image_layout = ImageLayout::DepthStencilAttachment,
            .load_op = clear_val ? AttachmentLoadOp::Clear : AttachmentLoadOp::Load,
            .store_op = use.access == PipelineAccess::DepthStencilWrite ? AttachmentStoreOp::Store : AttachmentStoreOp::None,
            .clear_value = { .depth_clear = clear_val.value_or(DepthClearValue{}) },
        };

        return info;
    }

    template<typename T>
    TaskImage &task_image_data(this TaskContext &self, T &use) {
        return self.task_graph.task_image_at(use.task_image_id);
    }

    template<typename T>
    void set_push_constants(this TaskContext &self, T &v) {
        ZoneScoped;

        PipelineLayoutID pipeline_layout = self.device.get_pipeline_layout<T>();
        self.cmd_list.set_push_constants(pipeline_layout, &v, sizeof(T), 0);
    }

    StagingBuffer &staging_buffer(this TaskContext &self) { return self.device.staging_buffer_at(self.frame_index); }

    Viewport pass_viewport(this TaskContext &self) {
        return {
            .x = 0,
            .y = 0,
            .width = static_cast<f32>(self.task.render_extent.width),
            .height = static_cast<f32>(self.task.render_extent.height),
            .depth_min = 0.01f,
            .depth_max = 1.0f,
        };
    }

    Rect2D pass_rect(this TaskContext &self) {
        return {
            .offset = { 0, 0 },
            .extent = self.task.render_extent,
        };
    }

    Device &device;
    TaskGraph &task_graph;
    Task &task;
    CommandList &cmd_list;
    CommandList &copy_cmd_list;
    usize frame_index;
};

}  // namespace lr
