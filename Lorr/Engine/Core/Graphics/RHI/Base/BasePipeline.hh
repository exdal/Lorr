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
        ResourceFormat Format = ResourceFormat::Unknown;

        bool BlendEnable = false;
        u8 WriteMask = 0;

        BlendFactor SrcBlend = BlendFactor::Zero;
        BlendFactor DstBlend = BlendFactor::Zero;
        BlendOp Blend = BlendOp::Add;

        BlendFactor SrcBlendAlpha = BlendFactor::Zero;
        BlendFactor DstBlendAlpha = BlendFactor::Zero;
        BlendOp BlendAlpha = BlendOp::Add;
    };

    struct DepthStencilOpDesc
    {
        StencilCompareOp Pass;
        StencilCompareOp Fail;

        CompareOp DepthFail;

        CompareOp CompareFunc;
    };

    struct PushConstantDesc
    {
        ShaderStage Stage = ShaderStage::None;
        u32 Offset = 0;
        u32 Size = 0;
    };

    /// Some notes:
    // * Currently only one vertex binding is supported and it only supports vertex, not instances (actually a TODO)
    // * MSAA is not actually supported
    // * Color blending stage is too restricted, only supporting one attachment
    struct GraphicsPipelineBuildInfo
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

        void SetDescriptorSets(std::initializer_list<BaseDescriptorSet *> sets, BaseDescriptorSet *pSamplerSet);
        PushConstantDesc &AddPushConstant();

        BaseDescriptorSet *m_pSamplerDescriptorSet = nullptr;
        u32 m_DescriptorSetCount = 0;
        BaseDescriptorSet *m_ppDescriptorSets[8];
        u32 m_PushConstantCount = 0;
        PushConstantDesc m_pPushConstants[8];
    };

    struct BasePipeline
    {
    };

}  // namespace lr::Graphics