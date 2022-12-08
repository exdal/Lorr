//
// Created on Thursday 27th October 2022 by e-erdal
//

#pragma once

#include "Core/Graphics/RHI/Base/BasePipeline.hh"

#include "D3D12Sym.hh"

namespace lr::Graphics
{
    struct D3D12Pipeline : BasePipeline
    {
        ID3D12PipelineState *pHandle = nullptr;
        ID3D12RootSignature *pLayout = nullptr;

        u32 m_pRootConstats[(u32)DescriptorType::Count] = {};
    };

    struct D3D12GraphicsPipelineBuildInfo : GraphicsPipelineBuildInfo
    {
        void Init();

        /// States - in order by member index
        // Shader Stages
        void SetShader(BaseShader *pShader, eastl::string_view entryPoint);

        // Vertex Input
        // `bindingID` always will be 0, since we only support one binding for now, see notes
        void SetInputLayout(InputLayout &inputLayout);

        // Input Assembly
        void SetPrimitiveType(PrimitiveType type);

        // Rasterizer
        void SetDepthClamp(bool enabled);
        void SetCullMode(CullMode mode, bool frontFaceClockwise);
        void SetFillMode(FillMode mode);
        void SetDepthBias(bool enabled, f32 constantFactor, f32 clamp, f32 slopeFactor);

        // Multisample
        void SetSampleCount(u32 bits);
        void SetAlphaToCoverage(bool alphaToCoverage);

        // Depth Stencil
        void SetDepthState(bool depthTestEnabled, bool depthWriteEnabled);
        void SetStencilState(bool stencilTestEnabled);
        void SetDepthFunction(CompareOp function);
        void SetStencilOperation(DepthStencilOpDesc front, DepthStencilOpDesc back);

        void AddAttachment(PipelineAttachment *pAttachment, bool depth);

        // Good thing that this create info takes pointers,
        // so we can easily enable/disable them by referencing them (nullptr means disabled)
        D3D12_GRAPHICS_PIPELINE_STATE_DESC m_CreateInfo;

        // Vars we will reference to create info
        D3D12_INPUT_ELEMENT_DESC m_pVertexAttribs[8];
    };

}  // namespace lr::Graphics