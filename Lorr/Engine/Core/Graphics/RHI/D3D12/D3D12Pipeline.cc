#include "D3D12Pipeline.hh"

#include "D3D12API.hh"
#include "D3D12Shader.hh"

namespace lr::Graphics
{
    constexpr D3D12_BLEND kBlendFactorLUT[] = {
        D3D12_BLEND_ZERO,              // Zero
        D3D12_BLEND_ONE,               // One
        D3D12_BLEND_SRC_COLOR,         // SrcColor
        D3D12_BLEND_INV_SRC_COLOR,     // InvSrcColor
        D3D12_BLEND_SRC_ALPHA,         // SrcAlpha
        D3D12_BLEND_INV_SRC_ALPHA,     // InvSrcAlpha
        D3D12_BLEND_DEST_ALPHA,        // DestAlpha
        D3D12_BLEND_INV_DEST_ALPHA,    // InvDestAlpha
        D3D12_BLEND_DEST_COLOR,        // DestColor
        D3D12_BLEND_INV_DEST_COLOR,    // InvDestColor
        D3D12_BLEND_SRC_ALPHA_SAT,     // SrcAlphaSat
        D3D12_BLEND_BLEND_FACTOR,      // ConstantColor
        D3D12_BLEND_INV_BLEND_FACTOR,  // InvConstantColor
        D3D12_BLEND_SRC1_COLOR,        // Src1Color
        D3D12_BLEND_INV_SRC1_COLOR,    // InvSrc1Color
        D3D12_BLEND_SRC1_ALPHA,        // Src1Alpha
        D3D12_BLEND_INV_SRC1_ALPHA,    // InvSrc1Alpha
    };

    void D3D12GraphicsPipelineBuildInfo::Init()
    {
        ZoneScoped;

        m_CreateInfo = {};

        m_CreateInfo.InputLayout.pInputElementDescs = m_pVertexAttribs;
        m_CreateInfo.SampleDesc.Count = 1;

        SetPrimitiveType(LR_PRIMITIVE_TYPE_TRIANGLE_LIST);

        SetSampleCount(1);
    }

    void D3D12GraphicsPipelineBuildInfo::SetShader(BaseShader *pShader, eastl::string_view entryPoint)
    {
        ZoneScoped;

        D3D12_SHADER_BYTECODE *pTargetHandle = nullptr;

        switch (pShader->Type)
        {
            case LR_SHADER_STAGE_VERTEX: pTargetHandle = &m_CreateInfo.VS; break;
            case LR_SHADER_STAGE_PIXEL: pTargetHandle = &m_CreateInfo.PS; break;
            case LR_SHADER_STAGE_HULL: pTargetHandle = &m_CreateInfo.HS; break;
            case LR_SHADER_STAGE_DOMAIN: pTargetHandle = &m_CreateInfo.DS; break;

            default: assert(!"Shader type not implemented"); break;
        }

        IDxcBlob *pCode = ((D3D12Shader *)pShader)->pHandle;

        pTargetHandle->pShaderBytecode = pCode->GetBufferPointer();
        pTargetHandle->BytecodeLength = pCode->GetBufferSize();
    }

    void D3D12GraphicsPipelineBuildInfo::SetInputLayout(InputLayout &inputLayout)
    {
        ZoneScoped;

        m_CreateInfo.InputLayout.NumElements = inputLayout.m_Count;

        for (u32 i = 0; i < inputLayout.m_Count; i++)
        {
            VertexAttrib &element = inputLayout.m_Elements[i];
            D3D12_INPUT_ELEMENT_DESC &attribDesc = m_pVertexAttribs[i];

            attribDesc.SemanticName = element.m_Name.data();
            attribDesc.SemanticIndex = 0;
            attribDesc.Format = D3D12API::ToDXFormat(element.m_Type);
            attribDesc.InputSlot = 0;
            attribDesc.AlignedByteOffset = element.m_Offset;
            attribDesc.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
            attribDesc.InstanceDataStepRate = 0;
        }
    }

    void D3D12GraphicsPipelineBuildInfo::SetPrimitiveType(PrimitiveType type)
    {
        ZoneScoped;

        // TODO: DXGI moment
        m_CreateInfo.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    }

    void D3D12GraphicsPipelineBuildInfo::SetDepthClamp(bool enabled)
    {
        ZoneScoped;

        m_CreateInfo.RasterizerState.DepthBiasClamp = enabled;
    }

    void D3D12GraphicsPipelineBuildInfo::SetCullMode(CullMode mode, bool frontFaceClockwise)
    {
        ZoneScoped;

        m_CreateInfo.RasterizerState.CullMode = D3D12API::ToDXCullMode(mode);
    }

    void D3D12GraphicsPipelineBuildInfo::SetFillMode(FillMode mode)
    {
        ZoneScoped;

        m_CreateInfo.RasterizerState.FillMode = mode == LR_FILL_MODE_FILL ? D3D12_FILL_MODE_SOLID : D3D12_FILL_MODE_WIREFRAME;
    }

    void D3D12GraphicsPipelineBuildInfo::SetDepthBias(bool enabled, f32 constantFactor, f32 clamp, f32 slopeFactor)
    {
        ZoneScoped;

        m_CreateInfo.RasterizerState.DepthBias = enabled;
        m_CreateInfo.RasterizerState.DepthBiasClamp = constantFactor;
        m_CreateInfo.RasterizerState.DepthBiasClamp = clamp;
        m_CreateInfo.RasterizerState.SlopeScaledDepthBias = slopeFactor;
    }

    void D3D12GraphicsPipelineBuildInfo::SetSampleCount(u32 bits)
    {
        ZoneScoped;

        m_CreateInfo.SampleMask = bits;
    }

    void D3D12GraphicsPipelineBuildInfo::SetAlphaToCoverage(bool alphaToCoverage)
    {
        ZoneScoped;

        m_CreateInfo.BlendState.AlphaToCoverageEnable = alphaToCoverage;
    }

    void D3D12GraphicsPipelineBuildInfo::SetDepthState(bool depthTestEnabled, bool depthWriteEnabled)
    {
        ZoneScoped;

        m_CreateInfo.DepthStencilState.DepthEnable = depthTestEnabled;
        m_CreateInfo.DepthStencilState.DepthWriteMask = (D3D12_DEPTH_WRITE_MASK)depthWriteEnabled;
    }

    void D3D12GraphicsPipelineBuildInfo::SetStencilState(bool stencilTestEnabled)
    {
        ZoneScoped;

        m_CreateInfo.DepthStencilState.StencilEnable = stencilTestEnabled;
    }

    void D3D12GraphicsPipelineBuildInfo::SetDepthFunction(CompareOp function)
    {
        ZoneScoped;

        m_CreateInfo.DepthStencilState.DepthFunc = (D3D12_COMPARISON_FUNC)function;
    }

    void D3D12GraphicsPipelineBuildInfo::SetStencilOperation(DepthStencilOpDesc front, DepthStencilOpDesc back)
    {
        ZoneScoped;

        m_CreateInfo.DepthStencilState.FrontFace.StencilFunc = (D3D12_COMPARISON_FUNC)front.CompareFunc;
        m_CreateInfo.DepthStencilState.FrontFace.StencilDepthFailOp = (D3D12_STENCIL_OP)front.DepthFail;
        m_CreateInfo.DepthStencilState.FrontFace.StencilFailOp = (D3D12_STENCIL_OP)front.Fail;
        m_CreateInfo.DepthStencilState.FrontFace.StencilPassOp = (D3D12_STENCIL_OP)front.Pass;

        m_CreateInfo.DepthStencilState.BackFace.StencilFunc = (D3D12_COMPARISON_FUNC)back.CompareFunc;
        m_CreateInfo.DepthStencilState.BackFace.StencilDepthFailOp = (D3D12_STENCIL_OP)back.DepthFail;
        m_CreateInfo.DepthStencilState.BackFace.StencilFailOp = (D3D12_STENCIL_OP)back.Fail;
        m_CreateInfo.DepthStencilState.BackFace.StencilPassOp = (D3D12_STENCIL_OP)back.Pass;
    }

    void D3D12GraphicsPipelineBuildInfo::AddAttachment(PipelineAttachment *pAttachment, bool depth)
    {
        ZoneScoped;

        if (depth)
        {
            m_CreateInfo.DSVFormat = D3D12API::ToDXFormat(pAttachment->Format);
            return;
        }

        u32 id = m_CreateInfo.NumRenderTargets++;

        m_CreateInfo.RTVFormats[id] = D3D12API::ToDXFormat(pAttachment->Format);

        D3D12_RENDER_TARGET_BLEND_DESC &desc = m_CreateInfo.BlendState.RenderTarget[id];

        desc.BlendEnable = pAttachment->BlendEnable;
        desc.RenderTargetWriteMask = pAttachment->WriteMask;

        desc.SrcBlend = kBlendFactorLUT[(u32)pAttachment->SrcBlend];
        desc.DestBlend = kBlendFactorLUT[(u32)pAttachment->DstBlend];

        desc.SrcBlendAlpha = kBlendFactorLUT[(u32)pAttachment->SrcBlendAlpha];
        desc.DestBlendAlpha = kBlendFactorLUT[(u32)pAttachment->DstBlendAlpha];

        desc.BlendOp = (D3D12_BLEND_OP)((u32)pAttachment->ColorBlendOp + 1);
        desc.BlendOpAlpha = (D3D12_BLEND_OP)((u32)pAttachment->AlphaBlendOp + 1);
    }

    void D3D12ComputePipelineBuildInfo::Init()
    {
        ZoneScoped;
    }

    void D3D12ComputePipelineBuildInfo::SetShader(BaseShader *pShader, eastl::string_view entryPoint)
    {
        ZoneScoped;

        IDxcBlob *pCode = ((D3D12Shader *)pShader)->pHandle;

        m_CreateInfo.CS.pShaderBytecode = pCode->GetBufferPointer();
        m_CreateInfo.CS.BytecodeLength = pCode->GetBufferSize();
    }

}  // namespace lr::Graphics