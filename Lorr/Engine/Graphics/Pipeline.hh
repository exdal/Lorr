#pragma once

#include "APIObject.hh"
#include "Common.hh"

namespace lr::Graphics
{
struct ColorBlendAttachment : VkPipelineColorBlendAttachmentState
{
    ColorBlendAttachment() = default;
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

struct PipelineAttachmentInfo : VkPipelineRenderingCreateInfo
{
    PipelineAttachmentInfo() = default;
    PipelineAttachmentInfo(eastl::span<Format> color_attachment_formats, Format depth_attachment_format);
};

struct Shader;
struct PipelineShaderStageInfo : VkPipelineShaderStageCreateInfo
{
    PipelineShaderStageInfo() = default;
    PipelineShaderStageInfo(Shader *shader, eastl::string_view entry_point);
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

struct PipelineViewportStateInfo : VkPipelineViewportStateCreateInfo
{
    PipelineViewportStateInfo() = default;
    PipelineViewportStateInfo(eastl::span<VkViewport> viewports, eastl::span<VkRect2D> scissors);
};

using PipelineLayout = VkPipelineLayout;
LR_ASSIGN_OBJECT_TYPE(PipelineLayout, VK_OBJECT_TYPE_PIPELINE_LAYOUT);

struct GraphicsPipelineInfo
{
    // Vertex input state
    eastl::vector<PipelineShaderStageInfo> m_shader_stages = {};
    eastl::vector<PipelineVertexLayoutBindingInfo> m_vertex_binding_infos = {};
    eastl::vector<PipelineVertexAttribInfo> m_vertex_attrib_infos = {};
    eastl::vector<VkViewport> m_viewports = {};
    eastl::vector<VkRect2D> m_scissors = {};
    // Rasterizer state
    PrimitiveType m_primitive_type : 8 = PrimitiveType::TriangleList;
    bool m_enable_depth_clamp : 1 = false;
    bool m_wireframe : 1 = false;
    CullMode m_cull_mode : 2 = {};
    bool m_front_face_ccw : 1 = false;
    bool m_enable_depth_bias : 1 = false;
    f32 m_depth_slope_factor = 0.0;
    f32 m_depth_bias_clamp = 0.0;
    f32 m_depth_bias_factor = 0.0;
    f32 m_line_width = 1.0;
    // Multisample state
    u32 m_multisample_bit_count = 1;
    bool m_enable_alpha_to_coverage : 1 = false;
    bool m_enable_alpha_to_one : 1 = false;
    // Depth stencil state
    bool m_enable_depth_test : 1 = false;
    bool m_enable_depth_write : 1 = false;
    bool m_enable_depth_bounds_test : 1 = false;
    bool m_enable_stencil_test : 1 = false;
    CompareOp m_depth_compare_op : 8 = {};
    DepthStencilOpDesc m_stencil_front_face_op = {};
    DepthStencilOpDesc m_stencil_back_face_op = {};
    // Color blend state
    eastl::vector<ColorBlendAttachment> m_blend_attachments = {};
    // Dynamic state
    eastl::vector<DynamicState> m_dynamic_states = {};

    PipelineLayout m_layout = nullptr;
};

struct ComputePipelineInfo
{
    PipelineShaderStageInfo m_shader_stage = {};

    PipelineLayout m_layout = nullptr;
};

struct Pipeline : Tracked<VkPipeline>
{
    PipelineLayout m_layout = {};
    PipelineBindPoint m_bind_point = PipelineBindPoint::Graphics;
};
LR_ASSIGN_OBJECT_TYPE(Pipeline, VK_OBJECT_TYPE_PIPELINE);

}  // namespace lr::Graphics