#include "ImGuiPass.hh"

#include <imgui.h>

namespace lr::Renderer
{
    void ImGuiPass::Init(BaseAPI *pAPI)
    {
        ZoneScoped;

        auto &io = ImGui::GetIO();

        /// EXTRACT TEXTURE BUFFER

        u8 *pFontData = nullptr;
        i32 fontW, fontH, bpp;
        io.Fonts->GetTexDataAsRGBA32(&pFontData, &fontW, &fontH, &bpp);

        BufferDesc imageBufDesc = {};
        imageBufDesc.Mappable = true;
        imageBufDesc.UsageFlags = ResourceUsage::CopySrc;
        imageBufDesc.TargetAllocator = AllocatorType::None;

        BufferData imageBufData = {};
        imageBufData.DataLen = fontW * fontH * bpp;

        BaseBuffer *pImageBuffer = pAPI->CreateBuffer(&imageBufDesc, &imageBufData);

        void *pMapData = nullptr;
        pAPI->MapMemory(pImageBuffer, pMapData);
        memcpy(pMapData, pFontData, imageBufData.DataLen);
        pAPI->UnmapMemory(pImageBuffer);

        /// CREATE TEXTURE ITSELF

        ImageDesc imageDesc = {};
        imageDesc.Format = ResourceFormat::RGBA8F;
        imageDesc.Usage = ResourceUsage::ShaderResource | ResourceUsage::CopyDst;
        imageDesc.ArraySize = 1;
        imageDesc.MipMapLevels = 1;
        imageDesc.TargetAllocator = AllocatorType::ImageTLSF;

        ImageData imageData = {};
        imageData.Width = fontW;
        imageData.Height = fontH;
        imageData.pData = nullptr;
        imageData.DataLen = fontW * fontH * bpp;

        m_pImage = pAPI->CreateImage(&imageDesc, &imageData);

        /// COPY BUFFER TO IMAGE

        BaseCommandList *pList = pAPI->GetCommandList();
        pAPI->BeginCommandList(pList);

        pList->BarrierTransition(m_pImage, ResourceUsage::HostRead, ShaderStage::None, ResourceUsage::CopyDst, ShaderStage::None);
        pList->CopyBuffer(pImageBuffer, m_pImage);
        pList->BarrierTransition(m_pImage, ResourceUsage::CopyDst, ShaderStage::None, ResourceUsage::ShaderResource, ShaderStage::Pixel);

        pAPI->EndCommandList(pList);
        pAPI->ExecuteCommandList(pList, true);

        /// CREATE SAMPLER

        SamplerDesc samplerDesc = {};
        samplerDesc.MinFilter = Filtering::Nearest;
        samplerDesc.MagFilter = Filtering::Nearest;
        samplerDesc.AddressU = TextureAddressMode::ClampToEdge;
        samplerDesc.AddressV = TextureAddressMode::ClampToEdge;
        samplerDesc.MaxLOD = 1;

        m_pSampler = pAPI->CreateSampler(&samplerDesc);

        /// CREATE PIPELINE

        BaseShader *pVertexShader = pAPI->CreateShader(ShaderStage::Vertex, "imgui.v.hlsl");
        BaseShader *pPixelShader = pAPI->CreateShader(ShaderStage::Pixel, "imgui.p.hlsl");

        DescriptorSetDesc descriptorDesc = {};
        descriptorDesc.BindingCount = 3;

        descriptorDesc.pBindings[0].Type = DescriptorType::ShaderResourceView;
        descriptorDesc.pBindings[0].TargetShader = ShaderStage::Pixel;
        descriptorDesc.pBindings[0].ArraySize = 1;
        descriptorDesc.pBindings[0].pImage = m_pImage;

        descriptorDesc.pBindings[1].Type = DescriptorType::Sampler;
        descriptorDesc.pBindings[1].TargetShader = ShaderStage::Pixel;
        descriptorDesc.pBindings[1].ArraySize = 1;
        descriptorDesc.pBindings[1].pSampler = m_pSampler;

        descriptorDesc.pBindings[2].Type = DescriptorType::RootConstant;
        descriptorDesc.pBindings[2].TargetShader = ShaderStage::Vertex;
        descriptorDesc.pBindings[2].RootConstantOffset = 0;
        descriptorDesc.pBindings[2].RootConstantSize = sizeof(XMMATRIX);
        descriptorDesc.pBindings[2].ArraySize = 1;

        m_pDescriptorSet = pAPI->CreateDescriptorSet(&descriptorDesc);
        pAPI->UpdateDescriptorData(m_pDescriptorSet);

        InputLayout inputLayout = {
            { VertexAttribType::Vec2, "POSITION" },
            { VertexAttribType::Vec2, "TEXCOORD" },
            { VertexAttribType::Vec4U_Packed, "COLOR" },
        };

        GraphicsPipelineBuildInfo *pBuildInfo = pAPI->BeginPipelineBuildInfo();

        pBuildInfo->SetInputLayout(inputLayout);
        pBuildInfo->SetShader(pVertexShader, "main");
        pBuildInfo->SetShader(pPixelShader, "main");
        pBuildInfo->SetFillMode(FillMode::Fill);
        pBuildInfo->SetDepthState(false, false);
        pBuildInfo->SetDepthFunction(CompareOp::LessEqual);
        pBuildInfo->SetCullMode(CullMode::None, false);
        pBuildInfo->SetDescriptorSets({ m_pDescriptorSet });

        PipelineAttachment colorAttachment = {};
        colorAttachment.Format = ResourceFormat::RGBA8F;
        colorAttachment.BlendEnable = false;
        colorAttachment.WriteMask = 0xf;
        colorAttachment.SrcBlend = BlendFactor::SrcAlpha;
        colorAttachment.DstBlend = BlendFactor::InvSrcAlpha;
        colorAttachment.SrcBlendAlpha = BlendFactor::One;
        colorAttachment.DstBlendAlpha = BlendFactor::InvSrcAlpha;
        colorAttachment.Blend = BlendOp::Add;
        colorAttachment.BlendAlpha = BlendOp::Add;
        pBuildInfo->AddAttachment(&colorAttachment, false);

        m_pPipeline = pAPI->EndPipelineBuildInfo(pBuildInfo);
    }

    void ImGuiPass::Draw(BaseAPI *pAPI, XMMATRIX &mat2d)
    {
        ZoneScoped;
    }

}  // namespace lr::Renderer