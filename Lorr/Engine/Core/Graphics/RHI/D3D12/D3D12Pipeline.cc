#include "D3D12Pipeline.hh"

#include "D3D12API.hh"
#include "D3D12Shader.hh"

namespace lr::Graphics
{
    void D3D12GraphicsPipelineBuildInfo::Init()
    {
        ZoneScoped;

        m_CreateInfo = {};

        m_CreateInfo.InputLayout.pInputElementDescs = m_pVertexAttribs;

        SetPrimitiveType(PrimitiveType::TriangleList);

        SetSampleCount(1);
    }

    void D3D12GraphicsPipelineBuildInfo::SetShader(BaseShader *pShader, eastl::string_view entryPoint)
    {
        ZoneScoped;

        D3D12_SHADER_BYTECODE *pTargetHandle = nullptr;

        switch (pShader->Type)
        {
            case ShaderType::Vertex: pTargetHandle = &m_CreateInfo.VS; break;
            case ShaderType::Pixel: pTargetHandle = &m_CreateInfo.PS; break;
            case ShaderType::Hull: pTargetHandle = &m_CreateInfo.HS; break;
            case ShaderType::Domain: pTargetHandle = &m_CreateInfo.DS; break;
            case ShaderType::Geometry: pTargetHandle = &m_CreateInfo.GS; break;

            default: assert(!"Shader type not implemented"); break;
        }

        *pTargetHandle = ((D3D12Shader *)pShader)->pHandle;
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
        // m_CreateInfo.PrimitiveTopologyType = D3D12API::ToDXTopology(type);
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

    void D3D12GraphicsPipelineBuildInfo::SetDepthFunction(DepthCompareOp function)
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

    void D3D12GraphicsPipelineBuildInfo::SetBlendAttachment(u32 attachmentID, bool enabled, u8 mask)
    {
        ZoneScoped;

        D3D12_RENDER_TARGET_BLEND_DESC &desc = m_CreateInfo.BlendState.RenderTarget[attachmentID];

        desc.BlendEnable = enabled;
        desc.RenderTargetWriteMask = mask;

        desc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
        desc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;

        desc.SrcBlendAlpha = D3D12_BLEND_ONE;
        desc.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;

        desc.BlendOp = D3D12_BLEND_OP_ADD;
        desc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    }

}  // namespace lr::Graphics