#pragma once

#include "Common.hh"

namespace lr::graphics {
struct PipelineLayoutInfo {
    std::span<DescriptorSetLayout> layouts = {};
    std::span<PushConstantRange> push_constants = {};
};

struct PipelineLayout {
    VkPipelineLayout m_handle = nullptr;

    operator auto &() { return m_handle; }
    explicit operator bool() { return m_handle != nullptr; }
};

struct alignas(64) GraphicsPipelineInfo {
    RenderingAttachmentInfo attachment_info = {};
    ls::static_vector<Viewport, Limits::ColorAttachments> viewports = {};
    ls::static_vector<Rect2D, Limits::ColorAttachments> scissors = {};

    // Vertex Input State
    ls::static_vector<PipelineShaderStageInfo, 4> shader_stages = {};
    ls::static_vector<PipelineVertexLayoutBindingInfo, 4> vertex_binding_infos = {};
    ls::static_vector<PipelineVertexAttribInfo, 16> vertex_attrib_infos = {};

    // Rasterizer State
    bool enable_depth_clamp = false;
    bool enable_depth_bias = false;
    bool enable_wireframe = false;
    bool front_face_ccw = false;
    f32 depth_slope_factor = 0.0;
    f32 depth_bias_clamp = 0.0;
    f32 depth_bias_factor = 0.0;
    f32 line_width = 1.0;
    CullMode cull_mode = {};
    PrimitiveType primitive_type = PrimitiveType::TriangleList;
    // Multisample State
    u32 multisample_bit_count = 1;
    bool enable_alpha_to_coverage = false;
    bool enable_alpha_to_one = false;
    // Depth Stencil State
    bool enable_depth_test = false;
    bool enable_depth_write = false;
    bool enable_depth_bounds_test = false;
    bool enable_stencil_test = false;
    CompareOp depth_compare_op = {};
    StencilFaceOp stencil_front_face_op = {};
    StencilFaceOp stencil_back_face_op = {};
    // Color Blend Attachment State
    ls::static_vector<PipelineColorBlendAttachment, Limits::ColorAttachments> blend_attachments = {};
    glm::vec4 blend_constants = {};
    // Dynamic State
    DynamicState dynamic_state = {};

    // always get the layout from device
    PipelineLayout *layout = nullptr;
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
