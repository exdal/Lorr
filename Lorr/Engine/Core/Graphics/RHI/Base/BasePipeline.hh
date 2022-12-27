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
            u32 _u_data[3] = {};
            struct
            {
                ColorMask WriteMask : 4;
                bool BlendEnable : 1;

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

    struct BasePipelineBuildInfo
    {
        void SetDescriptorSets(const std::initializer_list<BaseDescriptorSet *> &sets, BaseDescriptorSet *pSamplerSet);
        PushConstantDesc &AddPushConstant();

        BaseDescriptorSet *m_pSamplerDescriptorSet = nullptr;
        u32 m_DescriptorSetCount = 0;
        BaseDescriptorSet *m_ppDescriptorSets[LR_MAX_DESCRIPTOR_SETS_PER_PIPELINE];
        u32 m_PushConstantCount = 0;
        PushConstantDesc m_pPushConstants[LR_MAX_PUSH_CONSTANTS_PER_PIPELINE];
    };

    /// Some notes:
    // * Currently only one vertex binding is supported and it only supports vertex, not instances (actually a TODO)
    // * MSAA is not actually supported
    struct GraphicsPipelineBuildInfo : BasePipelineBuildInfo
    {
        virtual void Init() = 0;

        /// States - in order by member index
        // Shader Stages
        virtual void SetShader(BaseShader *pShader, eastl::string_view entryPoint) = 0;

        // Vertex Input
        // `bindingID` always will be 0, since we only support one binding for now, see notes
        virtual void SetInputLayout(InputLayout &inputLayout) = 0;

        // Input Assembly
        virtual void SetPrimitiveType(PrimitiveType type) = 0;

        // Rasterizer
        virtual void SetDepthClamp(bool enabled) = 0;
        virtual void SetCullMode(CullMode mode, bool frontFaceClockwise) = 0;
        virtual void SetFillMode(FillMode mode) = 0;
        virtual void SetDepthBias(bool enabled, f32 constantFactor, f32 clamp, f32 slopeFactor) = 0;

        // Multisample
        virtual void SetSampleCount(u32 sampleCount) = 0;
        virtual void SetAlphaToCoverage(bool alphaToCoverage) = 0;

        // Depth Stencil
        virtual void SetDepthState(bool depthTestEnabled, bool depthWriteEnabled) = 0;
        virtual void SetStencilState(bool stencilTestEnabled) = 0;
        virtual void SetDepthFunction(CompareOp function) = 0;
        virtual void SetStencilOperation(DepthStencilOpDesc front, DepthStencilOpDesc back) = 0;

        virtual void AddAttachment(PipelineAttachment *pAttachment, bool depth) = 0;
    };

    struct ComputePipelineBuildInfo : BasePipelineBuildInfo
    {
        virtual void Init() = 0;

        // Shader Stages
        virtual void SetShader(BaseShader *pShader, eastl::string_view entryPoint) = 0;
    };

    enum PipelineType
    {
        LR_PIPELINE_TYPE_GRAPHICS,
        LR_PIPELINE_TYPE_COMPUTE,
    };

    struct BasePipeline
    {
        PipelineType Type = LR_PIPELINE_TYPE_GRAPHICS;
    };

}  // namespace lr::Graphics