#pragma once

#include "Engine/Graphics/Common.hh"
#include "Engine/Graphics/Device.hh"

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
        u32 index;
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
    using ColorAttachmentWrite = TaskImageUse<ImageLayout::ColorAttachment, PipelineAccess::ColorAttachmentReadWrite>;
    using ColorAttachmentRead = TaskImageUse<ImageLayout::ColorAttachment, PipelineAccess::ColorAttachmentRead>;
    using PixelReadOnly = TaskImageUse<ImageLayout::ColorReadOnly, PipelineAccess::PixelShaderRead>;
    using ComputeWrite = TaskImageUse<ImageLayout::General, PipelineAccess::ComputeWrite>;
    using ComputeRead = TaskImageUse<ImageLayout::General, PipelineAccess::ComputeRead>;
    using TransferRead = TaskImageUse<ImageLayout::TransferSrc, PipelineAccess::TransferRead>;
    using TransferWrite = TaskImageUse<ImageLayout::TransferDst, PipelineAccess::TransferWrite>;
};  // namespace Preset

LR_HANDLE(TaskID, u32);
struct TaskContext {
    TaskContext(std::span<TaskImage> task_images, CommandList &cmd_list, Device *device)
        : m_task_images(task_images),
          m_cmd_list(cmd_list),
          m_device(device)
    {
    }

    CommandList &command_list() { return m_cmd_list; }
    Device *device() { return m_device; }

    template<typename T>
    RenderingAttachmentInfo as_color_attachment(T &use, std::optional<ColorClearValue> clear_val = std::nullopt)
    {
        ZoneScoped;

        LR_ASSERT(use.image_layout == ImageLayout::ColorAttachment, "Rendering Attachment must have ColorAttachment layout!");

        RenderingAttachmentInfo info = {
            .image_view_id = m_task_images[static_cast<usize>(use.task_image_id)].image_view_id,
            .image_layout = ImageLayout::ColorAttachment,
            .load_op = clear_val ? AttachmentLoadOp::Clear : AttachmentLoadOp::Load,
            .store_op = use.access == PipelineAccess::ColorAttachmentWrite ? AttachmentStoreOp::Store : AttachmentStoreOp::None,
            .clear_value = { clear_val.value_or(ColorClearValue{}) },
        };

        return info;
    }

    template<typename T>
    TaskImage &task_image_data(T &use)
    {
        return m_task_images[static_cast<usize>(use.task_image_id)];
    }

    template<typename T>
    void set_push_constants(T &v)
    {
        PipelineLayout &pipeline_layout = *m_device->get_layout<T>();
        m_cmd_list.set_push_constants(pipeline_layout, &v, sizeof(T), 0);
    }

private:
    std::span<TaskImage> m_task_images;
    CommandList &m_cmd_list;
    Device *m_device;
};

struct Task {
    virtual ~Task() = default;
    virtual void execute(TaskContext &tc) = 0;

    usize m_pipeline_layout_index = 0;
    std::span<TaskUse> m_task_uses = {};
    std::string_view m_name = {};
    f64 m_start_ts = 0.0f;
    f64 m_end_ts = 0.0f;

    f64 execution_time() { return (m_end_ts - m_start_ts) / 1000000.f; }
};

template<typename TaskT>
concept TaskConcept = requires {
    TaskT{}.uses;
    TaskT{}.name;
} and requires(TaskContext &tc) { TaskT{}.execute(tc); };

template<TaskConcept TaskT>
struct TaskWrapper : Task {
    TaskWrapper(const TaskT &task)
        : m_task(task)
    {
        constexpr static bool has_push_constants = requires { TaskT{}.push_constants; };
        if constexpr (has_push_constants) {
            m_pipeline_layout_index = sizeof(typename TaskT::PushConstants) / sizeof(u32);
        }

        m_task_uses = { reinterpret_cast<TaskUse *>(&m_task.uses), m_task_use_count };
        m_name = m_task.name;
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
    f64 start_ts = 0.0f;
    f64 end_ts = 0.0f;

    f64 execution_time() { return (end_ts - start_ts) / 1000000.f; }
};

struct TaskSubmit {
    CommandType type = CommandType::Graphics;

    std::vector<TaskBatch> batches = {};
    std::array<CommandAllocator, Limits::FrameCount> command_allocators = {};
    std::array<TimestampQueryPool, Limits::FrameCount> query_pools = {};

    // Additional pipeline barriers that will be executed after
    std::vector<u32> additional_signal_barrier_indices = {};

    auto &frame_cmd_allocator(u32 frame_index) { return command_allocators[frame_index]; }
    auto &frame_query_pool(u32 frame_index) { return query_pools[frame_index]; }
};

}  // namespace lr::graphics
