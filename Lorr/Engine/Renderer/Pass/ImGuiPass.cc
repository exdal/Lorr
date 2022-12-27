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
        imageDesc.Format = LR_RESOURCE_FORMAT_RGBA8F;
        imageDesc.UsageFlags = LR_RESOURCE_USAGE_SHADER_RESOURCE | LR_RESOURCE_USAGE_TRANSFER_DST;
        imageDesc.ArraySize = 1;
        imageDesc.MipMapLevels = 1;

        ImageData imageData = {};
        imageData.TargetAllocator = AllocatorType::ImageTLSF;
        imageData.Width = fontW;
        imageData.Height = fontH;

        m_pFontImage = pAPI->CreateImage(&imageDesc, &imageData);

        BufferDesc imageBufferDesc = {};
        imageBufferDesc.UsageFlags = LR_RESOURCE_USAGE_TRANSFER_SRC;

        BufferData imageBufferData = {};
        imageBufferData.Mappable = true;
        imageBufferData.TargetAllocator = AllocatorType::BufferFrameTime;
        imageBufferData.DataLen = fontW * fontH * bpp;

        BaseBuffer *pImageBuffer = pAPI->CreateBuffer(&imageBufferDesc, &imageBufferData);

        void *pMapData = nullptr;
        pAPI->MapMemory(pImageBuffer, pMapData);
        memcpy(pMapData, pFontData, imageBufferData.DataLen);
        pAPI->UnmapMemory(pImageBuffer);

        BaseCommandList *pList = pAPI->GetCommandList();
        pAPI->BeginCommandList(pList);

        PipelineBarrier barrier = {
            .CurrentUsage = LR_RESOURCE_USAGE_SHADER_RESOURCE,
            .CurrentStage = LR_PIPELINE_STAGE_PIXEL_SHADER,
            .CurrentAccess = LR_PIPELINE_ACCESS_SHADER_READ,
            .NextUsage = LR_RESOURCE_USAGE_TRANSFER_DST,
            .NextStage = LR_PIPELINE_STAGE_HOST,
            .NextAccess = LR_PIPELINE_ACCESS_HOST_READ,
        };
        pList->SetImageBarrier(m_pFontImage, &barrier);

        pList->CopyBuffer(pImageBuffer, m_pFontImage);

        barrier = {
            .CurrentUsage = LR_RESOURCE_USAGE_TRANSFER_DST,
            .CurrentStage = LR_PIPELINE_STAGE_HOST,
            .CurrentAccess = LR_PIPELINE_ACCESS_HOST_READ,
            .NextUsage = LR_RESOURCE_USAGE_SHADER_RESOURCE,
            .NextStage = LR_PIPELINE_STAGE_PIXEL_SHADER,
            .NextAccess = LR_PIPELINE_ACCESS_SHADER_READ,
        };
        pList->SetImageBarrier(m_pFontImage, &barrier);

        pAPI->EndCommandList(pList);
        pAPI->ExecuteCommandList(pList, true);

        pAPI->DeleteBuffer(pImageBuffer);

        /// CREATE PIPELINE

        SamplerDesc samplerDesc = {};
        samplerDesc.MinFilter = LR_FILTERING_LINEAR;
        samplerDesc.MagFilter = LR_FILTERING_LINEAR;
        samplerDesc.MipFilter = LR_FILTERING_NEAREST;
        samplerDesc.AddressU = LR_TEXTURE_ADDRESS_WRAP;
        samplerDesc.AddressV = LR_TEXTURE_ADDRESS_WRAP;
        samplerDesc.MaxLOD = 0;

        BaseShader *pVertexShader = pAPI->CreateShader(LR_SHADER_STAGE_VERTEX, "imgui.v.hlsl");
        BaseShader *pPixelShader = pAPI->CreateShader(LR_SHADER_STAGE_PIXEL, "imgui.p.hlsl");

        DescriptorSetDesc descriptorDesc = {};
        descriptorDesc.BindingCount = 1;

        descriptorDesc.pBindings[0] = {
            .BindingID = 0,
            .Type = LR_DESCRIPTOR_TYPE_SHADER_RESOURCE,
            .TargetShader = LR_SHADER_STAGE_PIXEL,
            .ArraySize = 1,
            .pImage = m_pFontImage,
        };

        m_pDescriptorSet = pAPI->CreateDescriptorSet(&descriptorDesc);
        pAPI->UpdateDescriptorData(m_pDescriptorSet);

        descriptorDesc.pBindings[0] = {
            .BindingID = 0,
            .Type = LR_DESCRIPTOR_TYPE_SAMPLER,
            .TargetShader = LR_SHADER_STAGE_PIXEL,
            .ArraySize = 1,
            .pSamplerInfo = &samplerDesc,
        };

        m_pSamplerSet = pAPI->CreateDescriptorSet(&descriptorDesc);

        InputLayout inputLayout = {
            { VertexAttribType::Vec2, "POSITION" },
            { VertexAttribType::Vec2, "TEXCOORD" },
            { VertexAttribType::Vec4U_Packed, "COLOR" },
        };

        GraphicsPipelineBuildInfo *pBuildInfo = pAPI->BeginGraphicsPipelineBuildInfo();

        pBuildInfo->SetInputLayout(inputLayout);
        pBuildInfo->SetShader(pVertexShader, "main");
        pBuildInfo->SetShader(pPixelShader, "main");
        pBuildInfo->SetFillMode(LR_FILL_MODE_FILL);
        pBuildInfo->SetDepthState(false, false);
        pBuildInfo->SetDepthFunction(LR_COMPARE_OP_NEVER);
        pBuildInfo->SetCullMode(LR_CULL_MODE_NONE, false);
        pBuildInfo->SetDescriptorSets({ m_pDescriptorSet }, m_pSamplerSet);

        PipelineAttachment colorAttachment = {};
        colorAttachment.Format = pAPI->GetSwapChain()->m_ImageFormat;
        colorAttachment.BlendEnable = true;
        colorAttachment.WriteMask = LR_COLOR_MASK_R | LR_COLOR_MASK_G | LR_COLOR_MASK_B | LR_COLOR_MASK_A;
        colorAttachment.SrcBlend = LR_BLEND_FACTOR_SRC_ALPHA;
        colorAttachment.DstBlend = LR_BLEND_FACTOR_INV_SRC_ALPHA;
        colorAttachment.SrcBlendAlpha = LR_BLEND_FACTOR_ONE;
        colorAttachment.DstBlendAlpha = LR_BLEND_FACTOR_SRC_ALPHA;
        colorAttachment.ColorBlendOp = LR_BLEND_OP_ADD;
        colorAttachment.AlphaBlendOp = LR_BLEND_OP_ADD;
        pBuildInfo->AddAttachment(&colorAttachment, false);

        PushConstantDesc &cameraConstant = pBuildInfo->AddPushConstant();
        cameraConstant.Offset = 0;
        cameraConstant.Size = sizeof(XMMATRIX);
        cameraConstant.Stage = LR_SHADER_STAGE_VERTEX;

        m_pPipeline = pAPI->EndGraphicsPipelineBuildInfo(pBuildInfo);
    }  // namespace lr::Renderer

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

            bufferData.Mappable = true;
            bufferData.TargetAllocator = AllocatorType::BufferFrameTime;

            // Vertex buffer

            m_VertexCount = pDrawData->TotalVtxCount + 1500;

            if (m_pVertexBuffer)
                pAPI->DeleteBuffer(m_pVertexBuffer);

            bufferDesc.UsageFlags = LR_RESOURCE_USAGE_VERTEX_BUFFER;
            bufferData.Stride = sizeof(ImDrawVert);
            bufferData.DataLen = m_VertexCount * sizeof(ImDrawVert);

            m_pVertexBuffer = pAPI->CreateBuffer(&bufferDesc, &bufferData);

            // Index buffer

            m_IndexCount = pDrawData->TotalIdxCount + 1500;

            if (m_pIndexBuffer)
                pAPI->DeleteBuffer(m_pIndexBuffer);

            bufferDesc.UsageFlags = LR_RESOURCE_USAGE_INDEX_BUFFER;
            bufferData.Stride = sizeof(ImDrawIdx);
            bufferData.DataLen = m_IndexCount * sizeof(ImDrawIdx);

            m_pIndexBuffer = pAPI->CreateBuffer(&bufferDesc, &bufferData);
        }

        // Map memory

        u32 mapDataOffset = 0;
        void *pMapData = nullptr;

        pAPI->MapMemory(m_pVertexBuffer, pMapData);
        for (u32 i = 0; i < pDrawData->CmdListsCount; i++)
        {
            const ImDrawList *pDrawList = pDrawData->CmdLists[i];
            memcpy((ImDrawVert *)pMapData + mapDataOffset, pDrawList->VtxBuffer.Data, pDrawList->VtxBuffer.Size * sizeof(ImDrawVert));

            mapDataOffset += pDrawList->VtxBuffer.Size;
        }

        pAPI->UnmapMemory(m_pVertexBuffer);
        mapDataOffset = 0;

        pAPI->MapMemory(m_pIndexBuffer, pMapData);
        for (u32 i = 0; i < pDrawData->CmdListsCount; i++)
        {
            const ImDrawList *pDrawList = pDrawData->CmdLists[i];
            memcpy((ImDrawIdx *)pMapData + mapDataOffset, pDrawList->IdxBuffer.Data, pDrawList->IdxBuffer.Size * sizeof(ImDrawIdx));

            mapDataOffset += pDrawList->IdxBuffer.Size;
        }

        pAPI->UnmapMemory(m_pIndexBuffer);
        mapDataOffset = 0;

        BaseCommandList *pList = pAPI->GetCommandList();
        pAPI->BeginCommandList(pList);

        CommandListAttachment attachment;
        attachment.pHandle = pImage;
        attachment.LoadOp = LR_ATTACHMENT_OP_CLEAR;
        attachment.StoreOp = LR_ATTACHMENT_OP_STORE;
        attachment.ClearVal = ClearValue({ 0.0, 0.0, 0.0, 1.0 });

        CommandListBeginDesc beginDesc = {};
        beginDesc.RenderArea = { 0, 0, (u32)pDrawData->DisplaySize.x, (u32)pDrawData->DisplaySize.y };
        beginDesc.ColorAttachmentCount = 1;
        beginDesc.pColorAttachments[0] = attachment;
        pList->BeginPass(&beginDesc);

        pList->SetGraphicsPipeline(m_pPipeline);
        pList->SetGraphicsPushConstants(m_pPipeline, LR_SHADER_STAGE_VERTEX, &mat2d, sizeof(XMMATRIX));

        pList->SetVertexBuffer(m_pVertexBuffer);
        pList->SetIndexBuffer(m_pIndexBuffer, false);

        pList->SetViewport(0, 0, 0, pDrawData->DisplaySize.x, pDrawData->DisplaySize.y);

        XMFLOAT2 clipOffset = { pDrawData->DisplayPos.x, pDrawData->DisplayPos.y };
        XMFLOAT2 clipScale = { pDrawData->FramebufferScale.x, pDrawData->FramebufferScale.y };

        u32 vertexOffset = 0;
        u32 indexOffset = 0;
        for (u32 i = 0; i < pDrawData->CmdListsCount; i++)
        {
            const ImDrawList *pDrawList = pDrawData->CmdLists[i];

            for (auto &cmd : pDrawList->CmdBuffer)
            {
                XMUINT2 clipMin(cmd.ClipRect.x - clipOffset.x, cmd.ClipRect.y - clipOffset.y);
                XMUINT2 clipMax(cmd.ClipRect.z - clipOffset.x, cmd.ClipRect.w - clipOffset.y);

                if (clipMin.x < 0.0f)
                    clipMin.x = 0.0f;
                if (clipMin.y < 0.0f)
                    clipMin.y = 0.0f;
                if (clipMax.x > pDrawData->DisplaySize.x)
                    clipMax.x = pDrawData->DisplaySize.x;
                if (clipMax.y > pDrawData->DisplaySize.y)
                    clipMax.y = pDrawData->DisplaySize.y;
                if (clipMax.x <= clipMin.x || clipMax.y <= clipMin.y)
                    continue;

                pList->SetScissors(0, clipMin.x, clipMin.y, clipMax.x, clipMax.y);

                BaseDescriptorSet *pSet = (BaseDescriptorSet *)cmd.TextureId;
                if (!pSet)
                    pSet = m_pDescriptorSet;

                pList->SetGraphicsDescriptorSets({ pSet, m_pSamplerSet });

                pList->SetPrimitiveType(LR_PRIMITIVE_TYPE_TRIANGLE_LIST);
                pList->DrawIndexed(cmd.ElemCount, cmd.IdxOffset + indexOffset, cmd.VtxOffset + vertexOffset);
            }

            vertexOffset += pDrawList->VtxBuffer.Size;
            indexOffset += pDrawList->IdxBuffer.Size;
        }

        pList->SetScissors(0, 0, 0, pDrawData->DisplaySize.x, pDrawData->DisplaySize.y);
        pList->EndPass();

        pAPI->EndCommandList(pList);
        pAPI->ExecuteCommandList(pList, false);
    }

}  // namespace lr::Renderer