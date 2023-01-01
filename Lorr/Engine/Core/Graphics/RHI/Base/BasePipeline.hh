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
        ResourceFormat Format;

        union
        {
            u64 _u_data = {};
            struct
            {
                ColorMask WriteMask : 4;
                u32 BlendEnable : 1;

                BlendFactor SrcBlend : 5;
                BlendFactor DstBlend : 5;
                BlendOp ColorBlendOp : 4;

                BlendFactor SrcBlendAlpha : 5;
                BlendFactor DstBlendAlpha : 5;
                BlendOp AlphaBlendOp : 4;
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
                StencilOp Pass : 3;
                StencilOp Fail : 3;
                CompareOp DepthFail : 3;
                CompareOp CompareFunc : 3;
            };
        };
    };

    struct PushConstantDesc
    {
        ShaderStage Stage = LR_SHADER_STAGE_NONE;
        u32 Offset = 0;
        u32 Size = 0;
    };

    /// Some notes:
    // * Currently only one vertex binding is supported and it only supports vertex, not instances (actually a TODO)
    // * MSAA is not actually supported
    struct GraphicsPipelineBuildInfo
    {
        u32 RenderTargetCount = 0;
        PipelineAttachment pRenderTargets[LR_MAX_RENDER_TARGET_PER_PASS] = {};
        PipelineAttachment DepthAttachment = {};

        u32 ShaderCount = 0;
        BaseShader *ppShaders[LR_SHADER_STAGE_COUNT] = {};

        u32 DescriptorSetCount = 0;
        BaseDescriptorSet *ppDescriptorSets[LR_MAX_DESCRIPTOR_SETS_PER_PIPELINE];
        BaseDescriptorSet *pSamplerDescriptorSet = nullptr;

        u32 PushConstantCount = 0;
        PushConstantDesc pPushConstants[LR_MAX_PUSH_CONSTANTS_PER_PIPELINE];

        InputLayout *pInputLayout = nullptr;

        f32 DepthBiasFactor = 0.0;
        f32 DepthBiasClamp = 0.0;
        f32 DepthSlopeFactor = 0.0;
        DepthStencilOpDesc StencilFrontFaceOp = {};
        DepthStencilOpDesc StencilBackFaceOp = {};

        union
        {
            u32 _u_data = 0;
            struct
            {
                u32 EnableDepthClamp : 1;
                u32 EnableDepthBias : 1;
                u32 EnableDepthTest : 1;
                u32 EnableDepthWrite : 1;
                u32 EnableStencilTest : 1;
                CompareOp DepthCompareOp : 3;
                CullMode SetCullMode : 2;
                FillMode SetFillMode : 1;
                u32 FrontFaceCCW : 1;
                u32 EnableAlphaToCoverage : 1;
                u32 MultiSampleBitCount : 3;
            };
        };
    };

    struct ComputePipelineBuildInfo
    {
        BaseShader *pShader = nullptr;
        
        u32 DescriptorSetCount = 0;
        BaseDescriptorSet *ppDescriptorSets[LR_MAX_DESCRIPTOR_SETS_PER_PIPELINE];
        BaseDescriptorSet *pSamplerDescriptorSet = nullptr;

        u32 PushConstantCount = 0;
        PushConstantDesc pPushConstants[LR_MAX_PUSH_CONSTANTS_PER_PIPELINE];
    };

    struct BasePipeline
    {
    };

}  // namespace lr::Graphics