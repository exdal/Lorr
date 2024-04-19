#pragma once

#include "Common.hh"

namespace lr::graphics {
struct PipelineLayoutInfo {
    ls::static_vector<VkDescriptorSetLayout, 8> layouts = {};
    ls::static_vector<PushConstantRange, 8> push_constants = {};
};

struct PipelineLayout {
    PipelineLayout() = default;
    PipelineLayout(VkPipelineLayout layout)
        : m_handle(layout)
    {
    }

    VkPipelineLayout m_handle = nullptr;

    operator auto &() { return m_handle; }
    explicit operator bool() { return m_handle != nullptr; }
};

enum class PipelineEnableFlag : u32 {
    None = 0,
    // Rasterizer State
    DepthClamp = 1 << 0,
    Wireframe = 1 << 1,
    FrontFaceCCW = 1 << 2,
    DepthBias = 1 << 2,

    // Multisample State
    AlphaToCoverage = 1 << 3,
    AlphatoOne = 1 << 4,

    // Depth Stencil State
    DepthTest = 1 << 5,
    DepthWrite = 1 << 6,
    DepthBoundsTest = 1 << 7,
    StencilTest = 1 << 8,
};
LR_TYPEOP_ARITHMETIC(PipelineEnableFlag, PipelineEnableFlag, |);
LR_TYPEOP_ARITHMETIC_INT(PipelineEnableFlag, PipelineEnableFlag, &);

struct alignas(64) GraphicsPipelineInfo {
    RenderingAttachmentInfo attachment_info = {};
    ls::static_vector<Viewport, Limits::ColorAttachments> viewports = {};
    ls::static_vector<Rect2D, Limits::ColorAttachments> scissors = {};

    // Vertex Input State
    ls::static_vector<PipelineShaderStageInfo::VkType, 4> shader_stages = {};
    ls::static_vector<PipelineVertexLayoutBindingInfo::VkType, 4> vertex_binding_infos = {};
    ls::static_vector<PipelineVertexAttribInfo::VkType, 16> vertex_attrib_infos = {};

    // Rasterizer State
    f32 depth_slope_factor = 0.0;
    f32 depth_bias_clamp = 0.0;
    f32 depth_bias_factor = 0.0;
    f32 line_width = 1.0;
    CullMode cull_mode = {};
    PrimitiveType primitive_type = PrimitiveType::TriangleList;
    // Multisample State
    u32 multisample_bit_count = 1;
    // Depth Stencil State
    CompareOp depth_compare_op = {};
    StencilFaceOp stencil_front_face_op = {};
    StencilFaceOp stencil_back_face_op = {};
    // Color Blend Attachment State
    ls::static_vector<PipelineColorBlendAttachment::VkType, Limits::ColorAttachments> blend_attachments = {};
    glm::vec4 blend_constants = {};

    // Dynamic State
    DynamicState dynamic_state = {};
    PipelineEnableFlag enable_flags = PipelineEnableFlag::None;

    PipelineLayout &layout;
};

struct ComputePipelineInfo {
    PipelineShaderStageInfo compute_shader_info = {};
    PipelineLayout &layout;
};

struct Pipeline {
    Pipeline() = default;
    Pipeline(VkPipeline pipeline, PipelineBindPoint bind_point)
        : m_handle(pipeline),
          m_bind_point(bind_point)
    {
    }

    PipelineBindPoint m_bind_point = PipelineBindPoint::Graphics;
    VkPipeline m_handle = nullptr;

    constexpr static auto OBJECT_TYPE = VK_OBJECT_TYPE_PIPELINE;
    operator auto &() { return m_handle; }
    explicit operator bool() { return m_handle != nullptr; }
};

}  // namespace lr::graphics
