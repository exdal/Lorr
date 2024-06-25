#pragma once

#include "Common.hh"
#include "Descriptor.hh"

namespace lr {
struct PipelineLayoutInfo {
    ls::span<DescriptorSetLayout> layouts = {};
    ls::span<PushConstantRange> push_constants = {};
};

struct PipelineLayout {
    VkPipelineLayout handle = VK_NULL_HANDLE;

    constexpr static auto OBJECT_TYPE = VK_OBJECT_TYPE_PIPELINE_LAYOUT;
    operator auto &() { return handle; }
    explicit operator bool() { return handle != VK_NULL_HANDLE; }
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

struct RasterizerStateInfo {
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
};

struct MultisampleStateInfo {
    u32 multisample_bit_count = 1;
    bool enable_alpha_to_coverage = false;
    bool enable_alpha_to_one = false;
};

struct DepthStencilStateInfo {
    bool enable_depth_test = false;
    bool enable_depth_write = false;
    bool enable_depth_bounds_test = false;
    bool enable_stencil_test = false;
    CompareOp depth_compare_op = {};
    StencilFaceOp stencil_front_face_op = {};
    StencilFaceOp stencil_back_face_op = {};
};

struct GraphicsPipelineInfo {
    ls::span<Format> color_attachment_formats = {};
    Format depth_attachment_format = Format::Unknown;
    Format stencil_attachment_format = Format::Unknown;

    ls::span<Viewport> viewports = {};
    ls::span<Rect2D> scissors = {};

    // Vertex Input State
    ls::span<ShaderID> shader_ids = {};
    ls::span<VertexLayoutBindingInfo> vertex_binding_infos = {};
    ls::span<VertexAttribInfo> vertex_attrib_infos = {};

    RasterizerStateInfo rasterizer_state = {};
    MultisampleStateInfo multisample_state = {};
    DepthStencilStateInfo depth_stencil_state = {};
    // Color Blend Attachment State
    ls::span<PipelineColorBlendAttachment> blend_attachments = {};
    glm::vec4 blend_constants = {};
    // Dynamic State
    DynamicState dynamic_state = {};

    PipelineLayoutID layout_id = PipelineLayoutID::None;
};

struct ComputePipelineInfo {
    ShaderID shader_id = ShaderID::Invalid;
    PipelineLayoutID layout_id = PipelineLayoutID::None;
};

struct Pipeline {
    Pipeline() = default;
    Pipeline(PipelineBindPoint bind_point_, VkPipeline pipeline_)
        : bind_point(bind_point_),
          handle(pipeline_)
    {
    }

    PipelineBindPoint bind_point = PipelineBindPoint::Graphics;
    VkPipeline handle = VK_NULL_HANDLE;

    constexpr static auto OBJECT_TYPE = VK_OBJECT_TYPE_PIPELINE;
    operator auto &() { return handle; }
    explicit operator bool() { return handle != VK_NULL_HANDLE; }
};

}  // namespace lr
