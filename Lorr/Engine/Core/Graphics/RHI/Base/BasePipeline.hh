//
// Created on Monday 24th October 2022 by e-erdal
//

#pragma once

#include "Core/Graphics/RHI/Common.hh"
#include "Core/Graphics/InputLayout.hh"

#include "BaseResource.hh"
#include "BaseShader.hh"

namespace lr::Graphics
{
    struct PipelineAttachment
    {
        ImageFormat m_Format;

        union
        {
            u64 _u_data = {};
            struct
            {
                ColorMask m_WriteMask : 4;
                u32 m_BlendEnable : 1;

                BlendFactor m_SrcBlend : 5;
                BlendFactor m_DstBlend : 5;
                BlendOp m_ColorBlendOp : 4;

                BlendFactor m_SrcBlendAlpha : 5;
                BlendFactor m_DstBlendAlpha : 5;
                BlendOp m_AlphaBlendOp : 4;
            };
        };
    };

    struct DepthStencilOpDesc
    {
        union
        {
            u32 _u_data = 0;
            struct
            {
                StencilOp m_Pass : 3;
                StencilOp m_Fail : 3;
                CompareOp m_DepthFail : 3;
                CompareOp m_CompareFunc : 3;
            };
        };
    };

    struct PushConstantDesc
    {
        ShaderStage m_Stage = LR_SHADER_STAGE_NONE;
        u32 m_Offset = 0;
        u32 m_Size = 0;
    };

    /// Some notes:
    // * Currently only one vertex binding is supported and it only supports vertex, not instances (actually a TODO)
    // * MSAA is not actually supported
    struct GraphicsPipelineBuildInfo
    {
        u32 m_RenderTargetCount = 0;
        PipelineAttachment m_pRenderTargets[LR_MAX_RENDER_TARGET_PER_PASS] = {};
        PipelineAttachment m_DepthAttachment = {};

        u32 m_ShaderCount = 0;
        Shader *m_ppShaders[LR_SHADER_STAGE_COUNT] = {};

        u32 m_DescriptorSetCount = 0;
        DescriptorSet *m_ppDescriptorSets[LR_MAX_DESCRIPTOR_SETS_PER_PIPELINE];

        u32 m_PushConstantCount = 0;
        PushConstantDesc m_pPushConstants[LR_MAX_PUSH_CONSTANTS_PER_PIPELINE];

        InputLayout *m_pInputLayout = nullptr;

        f32 m_DepthBiasFactor = 0.0;
        f32 m_DepthBiasClamp = 0.0;
        f32 m_DepthSlopeFactor = 0.0;
        DepthStencilOpDesc m_StencilFrontFaceOp = {};
        DepthStencilOpDesc m_StencilBackFaceOp = {};

        union
        {
            u32 _u_data = 0;
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
    };

    struct ComputePipelineBuildInfo
    {
        Shader *m_pShader = nullptr;

        u32 m_DescriptorSetCount = 0;
        DescriptorSet *m_ppDescriptorSets[LR_MAX_DESCRIPTOR_SETS_PER_PIPELINE];
        DescriptorSet *m_pSamplerDescriptorSet = nullptr;

        u32 m_PushConstantCount = 0;
        PushConstantDesc m_pPushConstants[LR_MAX_PUSH_CONSTANTS_PER_PIPELINE];
    };

    struct Pipeline
    {
    };

}  // namespace lr::Graphics