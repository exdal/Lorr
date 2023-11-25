#pragma once

#include "APIObject.hh"
#include "Common.hh"

#include <EASTL/vector.h>

namespace lr::Graphics
{
struct ColorBlendAttachment : VkPipelineColorBlendAttachmentState
{
    ColorBlendAttachment(
        bool enabled,
        ColorMask write_mask = ColorMask::RGBA,
        BlendFactor src_blend = BlendFactor::SrcAlpha,
        BlendFactor dst_blend = BlendFactor::InvSrcAlpha,
        BlendOp blend_op = BlendOp::Add,
        BlendFactor src_blend_alpha = BlendFactor::One,
        BlendFactor dst_blend_alpha = BlendFactor::InvSrcAlpha,
        BlendOp blend_op_alpha = BlendOp::Add);
};

struct DepthStencilOpDesc
{
    StencilOp m_pass : 3;
    StencilOp m_fail : 3;
    CompareOp m_depth_fail : 3;
    CompareOp m_compare_func : 3;
};

struct PushConstantDesc : VkPushConstantRange
{
    PushConstantDesc(ShaderStage stage, u32 offset, u32 size);
};

struct Shader;
struct PipelineShaderStageInfo : VkPipelineShaderStageCreateInfo
{
    PipelineShaderStageInfo() = default;
    PipelineShaderStageInfo(Shader *shader, eastl::string_view entry_point);
};

struct PipelineAttachmentInfo : VkPipelineRenderingCreateInfo
{
    PipelineAttachmentInfo() = default;
    PipelineAttachmentInfo(eastl::span<Format> color_attachment_formats, Format depth_attachment_format);
};

struct PipelineVertexLayoutBindingInfo : VkVertexInputBindingDescription
{
    PipelineVertexLayoutBindingInfo() = default;
    PipelineVertexLayoutBindingInfo(u32 binding_id, u32 stride, bool is_instanced = false);
};

struct PipelineVertexAttribInfo : VkVertexInputAttributeDescription
{
    PipelineVertexAttribInfo() = default;
    PipelineVertexAttribInfo(u32 target_binding, u32 index, Format format, u32 offset);
};

using PipelineLayout = VkPipelineLayout;
LR_ASSIGN_OBJECT_TYPE(PipelineLayout, VK_OBJECT_TYPE_PIPELINE_LAYOUT);

struct GraphicsPipelineInfo
{
    // Vertex input state
    eastl::span<PipelineShaderStageInfo> m_shader_stages = {};
    eastl::span<PipelineVertexLayoutBindingInfo> m_vertex_binding_infos = {};
    eastl::span<PipelineVertexAttribInfo> m_vertex_attrib_infos = {};
    // Rasterizer state
    PrimitiveType m_primitive_type = PrimitiveType::TriangleList;
    bool m_enable_depth_clamp = false;
    FillMode m_fill_mode = {};
    CullMode m_cull_mode = {};
    bool m_front_face_ccw = false;
    bool m_enable_depth_bias = false;
    f32 m_depth_slope_factor = 0.0;
    f32 m_depth_bias_clamp = 0.0;
    f32 m_depth_bias_factor = 0.0;
    // Multisample state
    u32 m_multisample_bit_count = 1;
    bool m_enable_alpha_to_coverage = false;
    bool m_enable_alpha_to_one = false;
    // Depth stencil state
    bool m_enable_depth_test = false;
    bool m_enable_depth_write = false;
    CompareOp m_depth_compare_op = {};
    bool m_enable_depth_bounds_test = false;
    bool m_enable_stencil_test = false;
    DepthStencilOpDesc m_stencil_front_face_op = {};
    DepthStencilOpDesc m_stencil_back_face_op = {};
    // Color blend state
    eastl::span<ColorBlendAttachment> m_blend_attachments = {};
    // Dynamic state
    eastl::span<PipelineDynamicState> m_dynamic_states = {};

    PipelineLayout m_layout = nullptr;
};

struct ComputePipelineInfo
{
    PipelineShaderStageInfo m_shader_stage = {};

    PipelineLayout m_layout = nullptr;
};

struct Pipeline : APIObject
{
    Pipeline(VkPipeline handle)
        : m_handle(handle){};

    VkPipelineBindPoint m_bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS;
    VkPipeline m_handle = nullptr;

    operator VkPipeline &() { return m_handle; }
};
LR_ASSIGN_OBJECT_TYPE(Pipeline, VK_OBJECT_TYPE_PIPELINE);

}  // namespace lr::Graphics