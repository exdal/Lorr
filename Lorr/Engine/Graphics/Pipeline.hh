#pragma once

#include "Common.hh"

namespace lr::graphics {
struct PipelineLayoutInfo {
    std::span<DescriptorSetLayout> layouts = {};
    std::span<PushConstantRange> push_constants = {};
};

struct PipelineLayout {
    VkPipelineLayout m_handle = nullptr;

    constexpr static auto OBJECT_TYPE = VK_OBJECT_TYPE_PIPELINE_LAYOUT;
    operator auto &() { return m_handle; }
    explicit operator bool() { return m_handle != nullptr; }
};

struct VertexLayoutBindingInfo {
    u32 binding = 0;
    u32 stride = 0;
    VertexInputRate input_rate = VertexInputRate::Vertex;
};

struct VertexAttribInfo {
    Format format = Format::Unknown;
    u32 location = 0;
    u32 binding = 0;
    u32 offset = 0;
};

struct GraphicsPipelineInfo {
    std::span<Format> color_attachment_formats = {};
    Format depth_attachment_format = Format::Unknown;
    Format stencil_attachment_format = Format::Unknown;

    std::span<Viewport> viewports = {};
    std::span<Rect2D> scissors = {};

    // Vertex Input State
    std::span<ShaderID> shader_ids = {};
    std::span<VertexLayoutBindingInfo> vertex_binding_infos = {};
    std::span<VertexAttribInfo> vertex_attrib_infos = {};

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
    std::span<PipelineColorBlendAttachment> blend_attachments = {};
    glm::vec4 blend_constants = {};
    // Dynamic State
    DynamicState dynamic_state = {};

    // If left as `nullptr`, device will automatically select
    // size-0 push constant layout
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
