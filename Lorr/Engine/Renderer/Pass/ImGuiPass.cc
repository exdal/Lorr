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

        /// CREATE TEXTURE

        ImageDesc imageDesc = {};
        imageDesc.Format = ResourceFormat::RGBA8F;
        imageDesc.Usage = ResourceUsage::ShaderResource;
        imageDesc.ArraySize = 1;
        imageDesc.MipMapLevels = 1;
        imageDesc.TargetAllocator = AllocatorType::ImageTLSF;

        ImageData imageData = {};
        imageData.Width = fontW;
        imageData.Height = fontH;
        imageData.pData = pFontData;
        imageData.DataLen = fontW * fontH * bpp;

        m_pFontImage = pAPI->CreateImage(&imageDesc, &imageData);

        /// CREATE PIPELINE

        SamplerDesc samplerDesc = {};
        samplerDesc.MinFilter = Filtering::Linear;
        samplerDesc.MagFilter = Filtering::Linear;
        samplerDesc.MipFilter = Filtering::Nearest;
        samplerDesc.AddressU = TextureAddressMode::Wrap;
        samplerDesc.AddressV = TextureAddressMode::Wrap;
        samplerDesc.MaxLOD = 0;

        BaseShader *pVertexShader = pAPI->CreateShader(ShaderStage::Vertex, "imgui.v.hlsl");
        BaseShader *pPixelShader = pAPI->CreateShader(ShaderStage::Pixel, "imgui.p.hlsl");

        DescriptorSetDesc descriptorDesc = {};
        descriptorDesc.BindingCount = 1;

        descriptorDesc.pBindings[0].BindingID = 0;
        descriptorDesc.pBindings[0].Type = DescriptorType::ShaderResourceView;
        descriptorDesc.pBindings[0].TargetShader = ShaderStage::Pixel;
        descriptorDesc.pBindings[0].ArraySize = 1;
        descriptorDesc.pBindings[0].pImage = m_pFontImage;

        m_pDescriptorSet = pAPI->CreateDescriptorSet(&descriptorDesc);
        pAPI->UpdateDescriptorData(m_pDescriptorSet, &descriptorDesc);

        descriptorDesc.pBindings[0].BindingID = 0;
        descriptorDesc.pBindings[0].Type = DescriptorType::Sampler;
        descriptorDesc.pBindings[0].TargetShader = ShaderStage::Pixel;
        descriptorDesc.pBindings[0].ArraySize = 1;
        descriptorDesc.pBindings[0].Sampler = samplerDesc;

        m_pSamplerSet = pAPI->CreateDescriptorSet(&descriptorDesc);

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
        pBuildInfo->SetDepthFunction(CompareOp::Never);
        pBuildInfo->SetCullMode(CullMode::None, false);
        pBuildInfo->SetDescriptorSets({ m_pDescriptorSet }, m_pSamplerSet);

        PipelineAttachment colorAttachment = {};
        colorAttachment.Format = pAPI->GetSwapChain()->m_ImageFormat;
        colorAttachment.BlendEnable = true;
        colorAttachment.WriteMask = 0xf;
        colorAttachment.SrcBlend = BlendFactor::SrcAlpha;
        colorAttachment.DstBlend = BlendFactor::InvSrcAlpha;
        colorAttachment.SrcBlendAlpha = BlendFactor::One;
        colorAttachment.DstBlendAlpha = BlendFactor::InvSrcAlpha;
        colorAttachment.Blend = BlendOp::Add;
        colorAttachment.BlendAlpha = BlendOp::Add;
        pBuildInfo->AddAttachment(&colorAttachment, false);

        PushConstantDesc &cameraConstant = pBuildInfo->AddPushConstant();
        cameraConstant.Offset = 0;
        cameraConstant.Size = sizeof(XMMATRIX);
        cameraConstant.Stage = ShaderStage::Vertex;

        m_pPipeline = pAPI->EndPipelineBuildInfo(pBuildInfo);
    }

    void ImGuiPass::Draw(BaseAPI *pAPI, XMMATRIX &mat2d)
    {
        ZoneScoped;

        // TODO: Actual UI pass with it's own render target
        BaseImage *pImage = pAPI->GetSwapChain()->GetCurrentImage();

        ImDrawData *pDrawData = ImGui::GetDrawData();

        if (pDrawData->DisplaySize.x <= 0.0f || pDrawData->DisplaySize.y <= 0.0f)
            return;

        if (pDrawData->TotalIdxCount == 0)
            return;

        if (m_VertexCount < pDrawData->TotalVtxCount || m_IndexCount < pDrawData->TotalIdxCount)
        {
            BufferDesc bufferDesc = {};
            BufferData bufferData = {};

            bufferDesc.Mappable = true;
            bufferDesc.TargetAllocator = AllocatorType::None;

            // Vertex buffer

            m_VertexCount = pDrawData->TotalVtxCount + 1500;

            if (m_pVertexBuffer)
                pAPI->DeleteBuffer(m_pVertexBuffer);

            bufferDesc.UsageFlags = ResourceUsage::VertexBuffer;
            bufferData.Stride = sizeof(ImDrawVert);
            bufferData.DataLen = m_VertexCount * sizeof(ImDrawVert);

            m_pVertexBuffer = pAPI->CreateBuffer(&bufferDesc, &bufferData);

            // Index buffer

            m_IndexCount = pDrawData->TotalIdxCount + 1500;

            if (m_pIndexBuffer)
                pAPI->DeleteBuffer(m_pIndexBuffer);

            bufferDesc.UsageFlags = ResourceUsage::IndexBuffer;
            bufferData.Stride = sizeof(ImDrawIdx);
            bufferData.DataLen = m_IndexCount * sizeof(ImDrawIdx);

            m_pIndexBuffer = pAPI->CreateBuffer(&bufferDesc, &bufferData);
        }

        // Map memory

        u32 vertexOffset = 0;
        u32 indexOffset = 0;

        void *pVertexMapData = nullptr;
        void *pIndexMapData = nullptr;

        pAPI->MapMemory(m_pVertexBuffer, pVertexMapData);
        pAPI->MapMemory(m_pIndexBuffer, pIndexMapData);

        for (u32 i = 0; i < pDrawData->CmdListsCount; i++)
        {
            const ImDrawList *pDrawList = pDrawData->CmdLists[i];

            memcpy((ImDrawVert *)pVertexMapData + vertexOffset, pDrawList->VtxBuffer.Data, pDrawList->VtxBuffer.Size * sizeof(ImDrawVert));
            memcpy((ImDrawIdx *)pIndexMapData + indexOffset, pDrawList->IdxBuffer.Data, pDrawList->IdxBuffer.Size * sizeof(ImDrawIdx));

            vertexOffset += pDrawList->VtxBuffer.Size;
            indexOffset += pDrawList->IdxBuffer.Size;
        }

        pAPI->UnmapMemory(m_pVertexBuffer);
        pAPI->UnmapMemory(m_pIndexBuffer);

        BaseCommandList *pList = pAPI->GetCommandList();
        pAPI->BeginCommandList(pList);

        CommandListAttachment attachment;
        attachment.pHandle = pImage;
        attachment.LoadOp = AttachmentOperation::Clear;
        attachment.StoreOp = AttachmentOperation::Store;
        attachment.ClearVal = ClearValue({ 0.0, 0.0, 0.0, 1.0 });

        CommandListBeginDesc beginDesc = {};
        beginDesc.RenderArea = { 0, 0, (u32)pDrawData->DisplaySize.x, (u32)pDrawData->DisplaySize.y };
        beginDesc.ColorAttachmentCount = 1;
        beginDesc.pColorAttachments[0] = attachment;
        pList->BeginPass(&beginDesc);

        pList->SetPipeline(m_pPipeline);
        pList->SetPipelineDescriptorSets({ m_pDescriptorSet, m_pSamplerSet });
        pList->SetPushConstants(m_pPipeline, ShaderStage::Vertex, &mat2d, sizeof(XMMATRIX));

        pList->SetVertexBuffer(m_pVertexBuffer);
        pList->SetIndexBuffer(m_pIndexBuffer, false);

        pList->SetViewport(0, 0, 0, pDrawData->DisplaySize.x, pDrawData->DisplaySize.y);

        XMFLOAT2 clipOffset = { pDrawData->DisplayPos.x, pDrawData->DisplayPos.y };
        XMFLOAT2 clipScale = { pDrawData->FramebufferScale.x, pDrawData->FramebufferScale.y };

        for (u32 i = 0; i < pDrawData->CmdListsCount; i++)
        {
            const ImDrawList *pDrawList = pDrawData->CmdLists[i];

            for (auto &cmd : pDrawList->CmdBuffer)
            {
                if (cmd.UserCallback != nullptr)
                {
                    continue;
                }

                XMUINT2 clipMin(cmd.ClipRect.x - clipOffset.x, cmd.ClipRect.y - clipOffset.y);
                XMUINT2 clipMax(cmd.ClipRect.z - clipOffset.x, cmd.ClipRect.w - clipOffset.y);

                if (clipMin.x < 0.0f)
                    clipMin.x = 0.0f;
                if (clipMin.y < 0.0f)
                    clipMin.y = 0.0f;
                if (clipMax.x > pDrawData->DisplaySize.x)
                    clipMax.x = (float)pDrawData->DisplaySize.x;
                if (clipMax.y > pDrawData->DisplaySize.y)
                    clipMax.y = (float)pDrawData->DisplaySize.y;
                if (clipMax.x <= clipMin.x || clipMax.y <= clipMin.y)
                    continue;

                pList->SetScissors(0, clipMin.x, clipMin.y, clipMax.x, clipMax.y);
                pList->SetPrimitiveType(PrimitiveType::TriangleList);

                pList->DrawIndexed(cmd.ElemCount, cmd.IdxOffset, cmd.VtxOffset);
            }
        }

        pList->SetScissors(0, 0, 0, pDrawData->DisplaySize.x, pDrawData->DisplaySize.y);
        pList->EndPass();

        pAPI->EndCommandList(pList);
        pAPI->ExecuteCommandList(pList, false);
    }

}  // namespace lr::Renderer