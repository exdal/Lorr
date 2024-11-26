#pragma once

#include "Task.hh"

namespace lr {
struct TaskExecuteInfo {
    u32 frame_index = 0;
    void *execution_data = nullptr;  // This is custom user data
    ls::span<Semaphore> wait_semas = {};
    ls::span<Semaphore> signal_semas = {};
};

struct TaskGraphInfo {
    Device device = {};
    u32 staging_buffer_size = ls::mib_to_bytes(8);
    vk::BufferUsage staging_buffer_uses = vk::BufferUsage::None;
};

struct TaskGraph : Handle<TaskGraph> {
    auto static create(const TaskGraphInfo &info) -> TaskGraph;
    auto destroy() -> void;

    auto add_image(const TaskImageInfo &info) -> TaskImageID;
    auto set_image(TaskImageID task_image_id, const TaskImageInfo &info) -> void;

    auto new_submit(vk::CommandType type) -> usize;

    // Recording
    template<typename TaskT = InlineTask>
    auto add_task(const TaskT &task_info) -> TaskID {
        ZoneScoped;

        std::unique_ptr<Task> task = std::make_unique<TaskWrapper<TaskT>>(task_info);
        return this->add_task(std::move(task));
    }
    auto add_task(std::unique_ptr<Task> &&task) -> TaskID;
    auto present(TaskImageID dst_task_image_id) -> void;

    // Debug tools
    auto draw_profiler_ui() -> void;

    // Rendering
    auto execute(const TaskExecuteInfo &info) -> void;

    auto transfer_manager() -> TransferManager &;
    auto task(TaskID id) -> Task &;
    auto task_image(TaskImageID id) -> TaskImage &;
};

struct TaskContext {
    Device &device;
    TaskGraph &task_graph;
    Task &task;
    CommandList &cmd_list;
    ls::static_vector<GPUAllocation, 8> batch_allocations = {};

private:
    void *execution_data;

public:
    vk::Extent2D render_extent = {};
    ls::option<PipelineID> pipeline_id = ls::nullopt;
    ls::option<PipelineLayoutID> pipeline_layout_id = ls::nullopt;

    TaskContext(Device &device_, TaskGraph &task_graph_, Task &task_, CommandList &cmd_list_, void *execution_data_);
    ~TaskContext();

    vk::RenderingAttachmentInfo as_color_attachment(
        this TaskContext &, TaskUse &use, std::optional<vk::ColorClearValue> clear_val = std::nullopt);
    vk::RenderingAttachmentInfo as_depth_attachment(
        this TaskContext &, TaskUse &use, std::optional<vk::DepthClearValue> clear_val = std::nullopt);
    TaskUse &task_use(usize index);
    TaskImage &task_image_data(this TaskContext &, TaskUse &use);
    TaskImage &task_image_data(this TaskContext &, TaskImageID task_image_id);
    vk::Viewport pass_viewport(this TaskContext &);
    vk::Rect2D pass_rect(this TaskContext &);
    GPUAllocation transient_buffer(u64 size);
    ImageViewID image_view(TaskImageID id);

    void set_pipeline(this TaskContext &, PipelineID pipeline_id);

    template<typename T>
    void set_push_constants(this TaskContext &self, const T &v) {
        ZoneScoped;

        self.cmd_list.set_push_constants(&v, sizeof(T), 0);
    }

    template<typename T = void>
    T &exec_data_as() {
        return *static_cast<T *>(execution_data);
    }
};

}  // namespace lr
