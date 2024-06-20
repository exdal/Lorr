#pragma once

#include "Engine/Graphics/Common.hh"
#include "Engine/Graphics/Device.hh"

namespace lr::graphics {
struct TaskPersistentImageInfo {
    ImageID image_id = ImageID::Invalid;
    ImageViewID image_view_id = ImageViewID::Invalid;
    ImageLayout layout = ImageLayout::Undefined;
    PipelineAccessImpl access = PipelineAccess::TopOfPipe;
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
    using ColorReadOnly = TaskImageUse<ImageLayout::ColorReadOnly, PipelineAccess::FragmentShaderRead>;
    using ComputeWrite = TaskImageUse<ImageLayout::General, PipelineAccess::ComputeWrite>;
    using ComputeRead = TaskImageUse<ImageLayout::General, PipelineAccess::ComputeRead>;
    using BlitRead = TaskImageUse<ImageLayout::TransferSrc, PipelineAccess::BlitRead>;
    using BlitWrite = TaskImageUse<ImageLayout::TransferDst, PipelineAccess::BlitWrite>;

    using VertexBuffer = TaskBufferUse<PipelineAccess::VertexAttrib>;
    using IndexBuffer = TaskBufferUse<PipelineAccess::IndexAttrib>;
};  // namespace Preset

struct TaskPipelineInfo {
    union {
        GraphicsPipelineInfo m_graphics_info = {};
        ComputePipelineInfo m_compute_info;
    };

    std::vector<Viewport> m_viewports = {};
    std::vector<Rect2D> m_scissors = {};
    std::vector<VertexLayoutBindingInfo> m_vertex_binding_infos = {};
    std::vector<VertexAttribInfo> m_vertex_attrib_infos = {};
    std::vector<PipelineColorBlendAttachment> m_blend_attachments = {};
    std::vector<ShaderID> m_shader_ids = {};

    void set_dynamic_states(DynamicState states) { m_graphics_info.dynamic_state = states; }
    void set_viewport(const Viewport &viewport) { m_viewports.push_back(viewport); }
    void set_scissors(const Rect2D &rect) { m_scissors.push_back(rect); }
    void set_shader(ShaderID shader_id) { m_shader_ids.push_back(shader_id); }
    void set_rasterizer_state(const RasterizerStateInfo &info) { m_graphics_info.rasterizer_state = info; }
    void set_multisample_state(const MultisampleStateInfo &info) { m_graphics_info.multisample_state = info; }
    void set_depth_stencil_state(const DepthStencilStateInfo &info) { m_graphics_info.depth_stencil_state = info; }
    void set_blend_constants(const glm::vec4 &constants) { m_graphics_info.blend_constants = constants; }
    void set_blend_attachments(const std::vector<PipelineColorBlendAttachment> &infos) { m_blend_attachments = infos; }
    void set_blend_attachment_all(const PipelineColorBlendAttachment &info)
    {
        for (auto &v : m_blend_attachments) {
            v = info;
        }
    }
    void set_vertex_layout(const std::vector<VertexAttribInfo> &attribs)
    {
        u32 stride = 0;
        for (const VertexAttribInfo &attrib : attribs) {
            stride += format_to_size(attrib.format);
        }

        m_vertex_binding_infos.push_back({
            .binding = static_cast<u32>(m_vertex_binding_infos.size()),
            .stride = stride,
            .input_rate = VertexInputRate::Vertex,
        });

        m_vertex_attrib_infos.insert(m_vertex_attrib_infos.end(), attribs.begin(), attribs.end());
    }
};

struct TaskPrepareInfo {
    Device *device = nullptr;
    TaskPipelineInfo pipeline_info = {};
};

LR_HANDLE(TaskID, u32);
struct TaskContext {
    TaskContext(ls::span<TaskImage> task_images_, CommandList &cmd_list_, CommandList &copy_cmd_list_, Device &device_, usize frame_index_)
        : task_images(task_images_),
          cmd_list(cmd_list_),
          copy_cmd_list(copy_cmd_list_),
          device(device_),
          frame_index(frame_index_)
    {
    }

    template<typename T>
    RenderingAttachmentInfo as_color_attachment(this TaskContext &self, T &use, std::optional<ColorClearValue> clear_val = std::nullopt)
    {
        ZoneScoped;

        LR_ASSERT(use.image_layout == ImageLayout::ColorAttachment, "Rendering Attachment must have ColorAttachment layout!");

        RenderingAttachmentInfo info = {
            .image_view_id = self.task_image_data(use).image_view_id,
            .image_layout = ImageLayout::ColorAttachment,
            .load_op = clear_val ? AttachmentLoadOp::Clear : AttachmentLoadOp::Load,
            .store_op = use.access == PipelineAccess::ColorAttachmentWrite ? AttachmentStoreOp::Store : AttachmentStoreOp::None,
            .clear_value = { clear_val.value_or(ColorClearValue{}) },
        };

        return info;
    }

    template<typename T>
    TaskImage &task_image_data(this TaskContext &self, T &use)
    {
        return self.task_images[static_cast<usize>(use.task_image_id)];
    }

    template<typename T>
    void set_push_constants(this TaskContext &self, T &v)
    {
        ZoneScoped;

        PipelineLayoutID pipeline_layout = self.device.get_pipeline_layout<T>();
        self.cmd_list.set_push_constants(pipeline_layout, &v, sizeof(T), 0);
    }

    StagingBuffer &staging_buffer(this TaskContext &self) { return self.device.staging_buffer_at(self.frame_index); }

    ls::span<TaskImage> task_images;
    CommandList &cmd_list;
    CommandList &copy_cmd_list;
    Device &device;
    usize frame_index;
};

struct Task {
    virtual ~Task() = default;
    virtual bool prepare(TaskPrepareInfo &info) = 0;
    virtual void execute(TaskContext &tc) = 0;

    PipelineID m_pipeline_id = PipelineID::Invalid;
    PipelineLayoutID m_pipeline_layout_id = PipelineLayoutID::None;
    ls::span<TaskUse> m_task_uses = {};
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
        constexpr bool has_push_constants = requires { TaskT{}.push_constants; };
        if constexpr (has_push_constants) {
            m_pipeline_layout_id = static_cast<PipelineLayoutID>(sizeof(typename TaskT::PushConstants) / sizeof(u32));
        }

        m_task_uses = { reinterpret_cast<TaskUse *>(&m_task.uses), TASK_USE_COUNT };
        m_name = m_task.name;
    }

    bool prepare(TaskPrepareInfo &info) override
    {
        constexpr bool has_prepare = requires { TaskT{}.prepare(info); };
        if constexpr (has_prepare) {
            return m_task.prepare(info);
        }

        return false;
    }

    void execute(TaskContext &tc) override { m_task.execute(tc); }

    TaskT m_task = {};
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

}  // namespace lr::graphics
