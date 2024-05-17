#pragma once

#include "Graphics/Common.hh"

namespace lr::graphics {
struct TaskPersistentImageInfo {
    ImageID image_id = ImageID::Invalid;
    ImageViewID image_view_id = ImageViewID::Invalid;
    ImageLayout layout = ImageLayout::Undefined;
    PipelineAccessImpl access = PipelineAccess::None;
};

LR_HANDLE(TaskBufferID, u32);
struct TaskBuffer {
    BufferID buffer_id = BufferID::Invalid;
    PipelineAccessImpl last_access = PipelineAccess::None;
    u32 last_batch_index = 0;
    u32 last_submit_index = 0;
};

LR_HANDLE(TaskImageID, u32);
struct TaskImage {
    ImageID image_id = ImageID::Invalid;
    ImageViewID image_view_id = ImageViewID::Invalid;
    ImageLayout last_layout = ImageLayout::Undefined;
    PipelineAccessImpl last_access = PipelineAccess::None;
    u32 last_batch_index = 0;
    u32 last_submit_index = 0;
};

enum class TaskUseType : u32 { Buffer = 0, Image };
struct TaskUse {
    TaskUseType type = TaskUseType::Buffer;
    ImageLayout image_layout = ImageLayout::Undefined;
    PipelineAccessImpl access = PipelineAccess::None;
    union {
        TaskBufferID task_buffer_id = TaskBufferID::Invalid;
        TaskImageID task_image_id;
    };
};

template<PipelineAccessImpl AccessT>
struct TaskBufferUse : TaskUse {
    constexpr TaskBufferUse()
    {
        this->type = TaskUseType::Buffer;
        this->access = AccessT;
    }

    constexpr TaskBufferUse(TaskBufferID id)
    {
        this->type = TaskUseType::Buffer;
        this->access = AccessT;
        this->task_buffer_id = id;
    };
};

template<ImageLayout LayoutT, PipelineAccessImpl AccessT>
struct TaskImageUse : TaskUse {
    constexpr TaskImageUse()
    {
        this->type = TaskUseType::Image;
        this->image_layout = LayoutT;
        this->access = AccessT;
    }

    constexpr TaskImageUse(TaskImageID id)
    {
        this->type = TaskUseType::Image;
        this->image_layout = LayoutT;
        this->access = AccessT;
        this->task_image_id = id;
    };
};

// Task Use Presets
namespace Preset {
    using ColorAttachmentRead = TaskImageUse<ImageLayout::ColorAttachment, PipelineAccess::ColorAttachmentRead>;
    using ColorAttachmentWrite = TaskImageUse<ImageLayout::ColorAttachment, PipelineAccess::ColorAttachmentWrite>;
    using PixelReadOnly = TaskImageUse<ImageLayout::ColorReadOnly, PipelineAccess::PixelShaderRead>;
    using ComputeReadOnly = TaskImageUse<ImageLayout::General, PipelineAccess::ComputeRead>;
};  // namespace Preset

LR_HANDLE(TaskID, u32);
struct TaskContext {
    TaskContext(std::span<TaskImage> task_images, CommandList &cmd_list)
        : m_task_images(task_images),
          m_cmd_list(cmd_list)
    {
    }

    CommandList &command_list() { return m_cmd_list; }

    template<typename T>
    RenderingAttachmentInfo as_color_attachment(T &use, std::optional<ColorClearValue> clear_val)
    {
        ZoneScoped;

        LR_ASSERT(use.image_layout == ImageLayout::ColorAttachment, "Rendering Attachment must have ColorAttachment layout!");
        LR_ASSERT(clear_val && use.access == PipelineAccess::ColorAttachmentWrite, "Cannot clear an attachment with read access");

        RenderingAttachmentInfo info = {
            .load_op = clear_val ? AttachmentLoadOp::Clear : AttachmentLoadOp::Load,
            .store_op = use.access == PipelineAccess::ColorAttachmentWrite ? AttachmentStoreOp::Store : AttachmentStoreOp::DontCare,
            .image_layout = ImageLayout::ColorAttachment,
            .image_view_id = m_task_images[static_cast<usize>(use.task_image_id)].image_view_id,
            .clear_value = clear_val.value_or(ColorClearValue{}),
        };

        return info;
    }

private:
    std::span<TaskImage> m_task_images;
    CommandList &m_cmd_list;
};

struct Task {
    virtual ~Task() = default;
    virtual void execute(TaskContext &tc) = 0;

    std::span<TaskUse> m_task_uses = {};
};

template<typename TaskT>
concept TaskConcept = requires { TaskT{}.uses; } and requires(TaskContext &tc) { TaskT{}.execute(tc); };

template<TaskConcept TaskT>
struct TaskWrapper : Task {
    TaskWrapper(const TaskT &task)
        : m_task(task)
    {
        m_task_uses = { reinterpret_cast<TaskUse *>(&m_task.uses), m_task_use_count };
    }

    void execute(TaskContext &tc) override { m_task.execute(tc); }

    TaskT m_task = {};
    constexpr static usize m_task_use_count = sizeof(typename TaskT::Uses) / sizeof(TaskUse);
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
};

struct TaskSubmit {
    CommandType type = CommandType::Graphics;

    std::vector<TaskBatch> batches = {};

    // Additional pipeline barriers that will be executed after
    std::vector<u32> additional_signal_barrier_indices = {};
};

}  // namespace lr::graphics
