// Created on Wednesday September 21st 2022 by exdal
// Last modified on Monday August 28th 2023 by exdal

#pragma once

#include "APIAllocator.hh"
#include "Common.hh"

#include "Shader.hh"

namespace lr::Graphics
{
struct ColorBlendAttachment : VkPipelineColorBlendAttachmentState
{
    ColorBlendAttachment(
        bool enabled,
        ColorMask writeMask = ColorMask::RGBA,
        BlendFactor srcBlend = BlendFactor::SrcAlpha,
        BlendFactor dstBlend = BlendFactor::InvSrcAlpha,
        BlendOp blendOp = BlendOp::Add,
        BlendFactor srcBlendAlpha = BlendFactor::One,
        BlendFactor dstBlendAlpha = BlendFactor::InvSrcAlpha,
        BlendOp blendOpAlpha = BlendOp::Add);
};

struct DepthStencilOpDesc
{
    StencilOp m_Pass : 3;
    StencilOp m_Fail : 3;
    CompareOp m_DepthFail : 3;
    CompareOp m_CompareFunc : 3;
};

struct PushConstantDesc : VkPushConstantRange
{
    PushConstantDesc(ShaderStage stage, u32 offset, u32 size);
};

struct PipelineLayout : APIObject<VK_OBJECT_TYPE_PIPELINE_LAYOUT>
{
    VkPipelineLayout m_pHandle = nullptr;
};

struct Pipeline : APIObject<VK_OBJECT_TYPE_PIPELINE>
{
    VkPipelineBindPoint m_BindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    VkPipeline m_pHandle = nullptr;
    PipelineLayout *m_pLayout = nullptr;
};

// TODO: Indirect drawing.
// TODO: Instances.
// TODO: MSAA.
struct GraphicsPipelineBuildInfo
{
    eastl::vector<Format> m_ColorAttachmentFormats = {};
    Format m_DepthAttachmentFormat = Format::Unknown;
    eastl::vector<ColorBlendAttachment> m_BlendAttachments = {};
    eastl::vector<Shader *> m_Shaders = {};
    PipelineLayout *m_pLayout = nullptr;
    f32 m_DepthBiasFactor = 0.0;
    f32 m_DepthBiasClamp = 0.0;
    f32 m_DepthSlopeFactor = 0.0;
    DepthStencilOpDesc m_StencilFrontFaceOp = {};
    DepthStencilOpDesc m_StencilBackFaceOp = {};
    u32 m_EnableDepthClamp : 1;
    u32 m_EnableDepthBias : 1;
    u32 m_EnableDepthTest : 1;
    u32 m_EnableDepthWrite : 1;
    u32 m_EnableStencilTest : 1;
    CompareOp m_DepthCompareOp : 3;
    CullMode m_SetCullMode : 2;
    FillMode m_SetFillMode : 1;
    u32 m_FrontFaceCCW : 1;
    u32 m_EnableAlphaToCoverage : 1;
    u32 m_MultiSampleBitCount : 3 = 1;
};

struct ComputePipelineBuildInfo
{
    Shader *m_pShader = nullptr;
    PipelineLayout *m_pLayout = nullptr;
};

}  // namespace lr::Graphics