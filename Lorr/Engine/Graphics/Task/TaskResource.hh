#pragma once

#include "Engine/Graphics/Common.hh"
#include "Engine/Graphics/Pipeline.hh"

namespace lr {
struct TaskImageInfo {
    ImageID image_id = ImageID::Invalid;
    ImageViewID image_view_id = ImageViewID::Invalid;
    ImageLayout layout = ImageLayout::Undefined;
    PipelineAccessImpl access = PipelineAccess::TopOfPipe;
    usize last_submit_index = 0;
};

struct TaskBufferInfo {
    BufferID buffer_id = BufferID::Invalid;
    PipelineAccessImpl last_access = PipelineAccess::None;
};

enum class TaskImageFlag {
    None = 0,
    SwapChainRelative = 1 << 0,
};

template<>
struct has_bitmask<TaskImageFlag> : std::true_type {};

struct TaskImage {
    ImageID image_id = ImageID::Invalid;
    ImageViewID image_view_id = ImageViewID::Invalid;
    ImageSubresourceRange subresource_range = {};
    TaskImageFlag flags = TaskImageFlag::None;
    ImageLayout last_layout = ImageLayout::Undefined;
    PipelineAccessImpl last_access = PipelineAccess::None;

    usize last_submit_index = 0;
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
    constexpr TaskBufferUse() {
        this->type = TaskUseType::Buffer;
        this->access = AccessT;
    }

    constexpr TaskBufferUse(TaskBufferID id) {
        this->type = TaskUseType::Buffer;
        this->access = AccessT;
        this->task_buffer_id = id;
    };
};

template<ImageLayout LayoutT, PipelineAccessImpl AccessT>
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
    using ColorAttachmentWrite = TaskImageUse<ImageLayout::ColorAttachment, PipelineAccess::ColorAttachmentReadWrite>;
    using ColorAttachmentRead = TaskImageUse<ImageLayout::ColorAttachment, PipelineAccess::ColorAttachmentRead>;
    using DepthAttachmentWrite = TaskImageUse<ImageLayout::DepthAttachment, PipelineAccess::DepthStencilReadWrite>;
    using DepthAttachmentRead = TaskImageUse<ImageLayout::DepthAttachment, PipelineAccess::DepthStencilRead>;
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
        GraphicsPipelineInfo graphics_info = {};
        ComputePipelineInfo compute_info;
    };

    std::vector<Viewport> viewports = {};
    std::vector<Rect2D> scissors = {};
    std::vector<VertexLayoutBindingInfo> vertex_binding_infos = {};
    std::vector<VertexAttribInfo> vertex_attrib_infos = {};
    std::vector<PipelineColorBlendAttachment> blend_attachments = {};
    std::vector<ShaderID> shader_ids = {};

    void set_dynamic_states(DynamicState states) { graphics_info.dynamic_state = states; }
    void set_viewport(const Viewport &viewport) { viewports.push_back(viewport); }
    void set_scissors(const Rect2D &rect) { scissors.push_back(rect); }
    void set_shader(ShaderID shader_id) { shader_ids.push_back(shader_id); }
    void set_rasterizer_state(const RasterizerStateInfo &info) { graphics_info.rasterizer_state = info; }
    void set_multisample_state(const MultisampleStateInfo &info) { graphics_info.multisample_state = info; }
    void set_depth_stencil_state(const DepthStencilStateInfo &info) { graphics_info.depth_stencil_state = info; }
    void set_blend_constants(const glm::vec4 &constants) { graphics_info.blend_constants = constants; }
    void set_blend_attachments(const std::vector<PipelineColorBlendAttachment> &infos) { blend_attachments = infos; }
    void set_blend_attachment_all(const PipelineColorBlendAttachment &info) {
        for (auto &v : blend_attachments) {
            v = info;
        }
    }
    void set_vertex_layout(ls::span<const VertexAttribInfo> attribs) {
        u32 stride = 0;
        for (const VertexAttribInfo &attrib : attribs) {
            stride += format_to_size(attrib.format);
        }

        vertex_binding_infos.push_back({
            .binding = static_cast<u32>(vertex_binding_infos.size()),
            .stride = stride,
            .input_rate = VertexInputRate::Vertex,
        });

        vertex_attrib_infos.insert(vertex_attrib_infos.end(), attribs.begin(), attribs.end());
    }
};

}  // namespace lr
