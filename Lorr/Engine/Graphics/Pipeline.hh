#pragma once

#include "APIObject.hh"
#include "Common.hh"

#include <EASTL/vector.h>

#include "Shader.hh"

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

using PipelineLayout = VkPipelineLayout;
LR_ASSIGN_OBJECT_TYPE(PipelineLayout, VK_OBJECT_TYPE_PIPELINE_LAYOUT);

struct Pipeline : APIObject
{
    VkPipelineBindPoint m_bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS;
    VkPipeline m_handle = nullptr;

    operator VkPipeline &() { return m_handle; }
};
LR_ASSIGN_OBJECT_TYPE(Pipeline, VK_OBJECT_TYPE_PIPELINE);

// TODO: Indirect drawing.
// TODO: Instances.
// TODO: MSAA.
struct GraphicsPipelineBuildInfo
{
    eastl::span<Format> m_color_attachment_formats = {};
    Format m_depth_attachment_format = Format::Unknown;
    eastl::span<ColorBlendAttachment> m_blend_attachments = {};
    eastl::span<Shader *> m_shaders = {};
    PipelineLayout m_layout = nullptr;
    f32 m_depth_bias_factor = 0.0;
    f32 m_depth_bias_clamp = 0.0;
    f32 m_depth_slope_factor = 0.0;
    DepthStencilOpDesc m_stencil_front_face_op = {};
    DepthStencilOpDesc m_stencil_back_face_op = {};
    u32 m_enable_depth_clamp : 1;
    u32 m_enable_depth_bias : 1;
    u32 m_enable_depth_test : 1;
    u32 m_enable_depth_write : 1;
    u32 m_enable_stencil_test : 1;
    CompareOp m_depth_compare_op : 3;
    CullMode m_set_cull_mode : 2;
    FillMode m_set_fill_mode : 1;
    u32 m_front_face_ccw : 1;
    u32 m_enable_alpha_to_coverage : 1;
    u32 m_multi_sample_bit_count : 3 = 1;
};

struct ComputePipelineBuildInfo
{
    Shader *m_shader = nullptr;
    PipelineLayout m_layout = nullptr;
};

}  // namespace lr::Graphics