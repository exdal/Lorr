#pragma once

#include "Engine/Graphics/Vulkan.hh"

namespace lr {
enum class TaskImageID : u32 { Invalid = ~0_u32 };
struct TaskImageInfo {
    ImageID image_id = ImageID::Invalid;
    vk::ImageLayout layout = vk::ImageLayout::Undefined;
    vk::PipelineAccessImpl access = vk::PipelineAccess::TopOfPipe;
    usize last_submit_index = 0;
};

struct TaskImage {
    ImageID image_id = ImageID::Invalid;
    ImageViewID image_view_id = ImageViewID::Invalid;
    vk::ImageSubresourceRange subresource_range = {};
    vk::ImageLayout last_layout = vk::ImageLayout::Undefined;
    vk::PipelineAccessImpl last_access = vk::PipelineAccess::None;

    usize last_submit_index = 0;
};

enum class TaskUseType : u32 { Buffer = 0, Image };
struct TaskUse {
    TaskUseType type = TaskUseType::Buffer;
    vk::ImageLayout image_layout = vk::ImageLayout::Undefined;
    vk::PipelineAccessImpl access = vk::PipelineAccess::None;
    union {
        TaskImageID task_image_id = TaskImageID::Invalid;
        u32 index;
    };
};

template<vk::PipelineAccessImpl AccessT>
struct TaskBufferUse : TaskUse {
    constexpr TaskBufferUse() {
        this->type = TaskUseType::Buffer;
        this->access = AccessT;
    }
};

template<vk::ImageLayout LayoutT, vk::PipelineAccessImpl AccessT>
struct TaskImageUse : TaskUse {
    constexpr TaskImageUse() {
        this->type = TaskUseType::Image;
        this->image_layout = LayoutT;
        this->access = AccessT;
    }

    constexpr TaskImageUse(TaskImageID id) {
        this->type = TaskUseType::Image;
        this->image_layout = LayoutT;
        this->access = AccessT;
        this->task_image_id = id;
    };
};

// Task Use Presets
namespace Preset {
    using ColorAttachmentWrite = TaskImageUse<vk::ImageLayout::ColorAttachment, vk::PipelineAccess::ColorAttachmentReadWrite>;
    using ColorAttachmentRead = TaskImageUse<vk::ImageLayout::ColorAttachment, vk::PipelineAccess::ColorAttachmentRead>;
    using DepthAttachmentWrite = TaskImageUse<vk::ImageLayout::DepthAttachment, vk::PipelineAccess::DepthStencilReadWrite>;
    using DepthAttachmentRead = TaskImageUse<vk::ImageLayout::DepthAttachment, vk::PipelineAccess::DepthStencilRead>;
    using ColorReadOnly = TaskImageUse<vk::ImageLayout::ColorReadOnly, vk::PipelineAccess::ColorRead>;
    using ShaderReadOnly = TaskImageUse<vk::ImageLayout::ShaderReadOnly, vk::PipelineAccess::FragmentShaderRead>;
    using ComputeWrite = TaskImageUse<vk::ImageLayout::General, vk::PipelineAccess::ComputeWrite>;
    using ComputeRead = TaskImageUse<vk::ImageLayout::General, vk::PipelineAccess::ComputeRead>;
    using BlitRead = TaskImageUse<vk::ImageLayout::TransferSrc, vk::PipelineAccess::BlitRead>;
    using BlitWrite = TaskImageUse<vk::ImageLayout::TransferDst, vk::PipelineAccess::BlitWrite>;
};  // namespace Preset

enum class TaskID : u32 { Invalid = ~0_u32 };

struct TaskContext;
struct Task {
    virtual ~Task() = default;
    virtual void execute(TaskContext &tc) = 0;

    ls::span<TaskUse> task_uses = {};
    std::string name = {};
    f64 start_ts = 0.0f;
    f64 end_ts = 0.0f;
    u32 color = 0xFFFFFFFF;

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
concept IsTaskUSeContainer = requires() {
    std::begin(TaskT{}.uses);
    std::end(TaskT{}.uses);
};

template<TaskConcept TaskT>
struct TaskWrapper : Task {
    TaskWrapper(const TaskT &task_)
        : task(task_) {
        if constexpr (IsTaskUSeContainer<TaskT>) {
            task_uses = { task.uses.begin(), task.uses.end() };
        } else {
            constexpr usize TASK_USE_COUNT = sizeof(decltype(task.uses)) / sizeof(TaskUse);
            task_uses = { reinterpret_cast<TaskUse *>(&task.uses), TASK_USE_COUNT };
        }
        name = task.name;
    }

    void execute(TaskContext &tc) override { task.execute(tc); }

    TaskT task = {};
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
