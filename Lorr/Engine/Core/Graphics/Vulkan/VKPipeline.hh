//
// Created on Wednesday 21st September 2022 by exdal
//

#pragma once

#include "Allocator.hh"
#include "InputLayout.hh"
#include "Config.hh"

#include "VKResource.hh"
#include "VKShader.hh"

#include "Vulkan.hh"

namespace lr::Graphics
{
enum PrimitiveType : u32
{
    LR_PRIMITIVE_TYPE_POINT_LIST = 0,
    LR_PRIMITIVE_TYPE_LINE_LIST,
    LR_PRIMITIVE_TYPE_LINE_STRIP,
    LR_PRIMITIVE_TYPE_TRIANGLE_LIST,
    LR_PRIMITIVE_TYPE_TRIANGLE_STRIP,
    LR_PRIMITIVE_TYPE_PATCH,
};

enum CullMode : u32
{
    LR_CULL_MODE_NONE = 0,
    LR_CULL_MODE_FRONT,
    LR_CULL_MODE_BACK,
};

enum FillMode : u32
{
    LR_FILL_MODE_FILL = 0,
    LR_FILL_MODE_WIREFRAME,
};

struct PushConstantDesc
{
    ShaderStage m_Stage = LR_SHADER_STAGE_NONE;
    u32 m_Offset = 0;
    u32 m_Size = 0;
};

struct PipelineLayoutSerializeDesc
{
    eastl::span<DescriptorSetLayout *> m_Layouts;
    eastl::span<PushConstantDesc> m_PushConstants;
};

// TODO: Indirect drawing.
// TODO: Instances.
// TODO: MSAA.
struct GraphicsPipelineBuildInfo
{
    eastl::span<ImageFormat> m_ColorAttachmentFormats;
    ImageFormat m_DepthAttachmentFormat = LR_IMAGE_FORMAT_UNKNOWN;
    eastl::span<ColorBlendAttachment> m_BlendAttachments;
    eastl::span<Shader *> m_Shaders;
    eastl::span<DescriptorSetLayout *> m_Layouts;
    eastl::span<PushConstantDesc> m_PushConstants;
    InputLayout *m_pInputLayout = nullptr;
    f32 m_DepthBiasFactor = 0.0;
    f32 m_DepthBiasClamp = 0.0;
    f32 m_DepthSlopeFactor = 0.0;
    DepthStencilOpDesc m_StencilFrontFaceOp = {};
    DepthStencilOpDesc m_StencilBackFaceOp = {};

    struct
    {
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
        u32 m_MultiSampleBitCount : 3;
    };
};

struct ComputePipelineBuildInfo
{
    Shader *m_pShader = nullptr;
    eastl::span<DescriptorSetLayout *> m_Layouts;
    eastl::span<PushConstantDesc> m_PushConstants;
};

struct Pipeline : APIObject<VK_OBJECT_TYPE_PIPELINE>
{
    VkPipelineBindPoint m_BindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    VkPipeline m_pHandle = nullptr;
    VkPipelineLayout m_pLayout = nullptr;
};

}  // namespace lr::Graphics