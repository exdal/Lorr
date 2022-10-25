//
// Created on Monday 24th October 2022 by e-erdal
//

#pragma once

#include "Core/Graphics/RHI/Common.hh"
#include "Core/Graphics/InputLayout.hh"

#include "BaseRenderPass.hh"
#include "BaseShader.hh"

namespace lr::Graphics
{
    struct DepthStencilOpDesc
    {
        StencilCompareOp Pass;
        StencilCompareOp Fail;
        DepthCompareOp DepthFail;

        DepthCompareOp Function;
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
        virtual void SetShader(BaseShader *pShader) = 0;

        // Vertex Input
        // `bindingID` always will be 0, since we only support one binding for now, see notes
        virtual void SetInputLayout(u32 bindingID, InputLayout &inputLayout) = 0;

        // Input Assembly
        virtual void SetPrimitiveType(PrimitiveType type) = 0;

        // Tessellation
        virtual void SetPatchCount(u32 count) = 0;

        // Viewport
        virtual void AddViewport(u32 viewportCount = 1, u32 scissorCount = 0) = 0;
        virtual void SetViewport(u32 viewportID, u32 width, u32 height, f32 minDepth, f32 maxDepth) = 0;
        virtual void SetScissor(u32 scissorID, u32 x, u32 y, u32 w, u32 h) = 0;

        // Rasterizer
        virtual void SetDepthClamp(bool enabled) = 0;
        virtual void SetRasterizerDiscard(bool enabled) = 0;
        virtual void SetCullMode(CullMode mode, bool frontFaceClockwise) = 0;
        virtual void SetDepthBias(bool enabled, f32 constantFactor, f32 clamp, f32 slopeFactor) = 0;
        virtual void SetLineWidth(f32 size) = 0;

        // Multisample
        virtual void SetSampleCount(u32 sampleCount) = 0;
        virtual void SetSampledShading(bool enabled, f32 minSampleShading) = 0;
        virtual void SetAlphaToCoverage(bool alphaToCoverage, bool alphaToOne) = 0;

        // Depth Stencil
        virtual void SetDepthState(bool depthTestEnabled, bool depthWriteEnabled, bool depthBoundsTestEnabled) = 0;
        virtual void SetStencilState(bool stencilTestEnabled) = 0;
        virtual void SetStencilOperation(DepthStencilOpDesc front, DepthStencilOpDesc back) = 0;
        virtual void SetDepthBounds(f32 min, f32 max) = 0;

        // TODO: Color Blend
        virtual void SetBlendAttachment(u32 attachmentID, bool enabled, u8 mask) = 0;
        // void SetBlendState(bool enabled);
        // void SetBlendColorFactor(VkBlendFactor src, VkBlendFactor dst, VkBlendOp op);
        // void SetBlendAlphaFactor(VkBlendFactor src, VkBlendFactor dst, VkBlendOp op);
        // void SetBlendWriteState(bool writeR, bool writeG, bool writeB, bool writeA);
    };

    struct BasePipeline
    {
        BaseRenderPass *pBoundRenderPass = nullptr;
    };

}  // namespace lr::Graphics