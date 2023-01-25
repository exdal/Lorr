#include "ImGuiPass.hh"

#include "Resources/imgui.v.hh"
#include "Resources/imgui.p.hh"

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

        ImageDesc imageDesc = {
            .m_UsageFlags = LR_RESOURCE_USAGE_SHADER_RESOURCE | LR_RESOURCE_USAGE_TRANSFER_DST,
            .m_Format = LR_IMAGE_FORMAT_RGBA8F,
            .m_TargetAllocator = LR_API_ALLOCATOR_IMAGE_TLSF,
            .m_Width = (u16)fontW,
            .m_Height = (u16)fontH,
            .m_ArraySize = 1,
            .m_MipMapLevels = 1,
        };

        m_pFontImage = pAPI->CreateImage(&imageDesc);

        BufferDesc imageBufferDesc = {
            .m_UsageFlags = LR_RESOURCE_USAGE_TRANSFER_SRC | LR_RESOURCE_USAGE_HOST_VISIBLE,
            .m_TargetAllocator = LR_API_ALLOCATOR_BUFFER_FRAMETIME,
            .m_DataLen = (u32)(fontW * fontH * bpp),
        };

        Buffer *pImageBuffer = pAPI->CreateBuffer(&imageBufferDesc);

        void *pMapData = nullptr;
        pAPI->MapMemory(pImageBuffer, pMapData);
        memcpy(pMapData, pFontData, imageBufferDesc.m_DataLen);
        pAPI->UnmapMemory(pImageBuffer);

        CommandList *pList = pAPI->GetCommandList();
        pAPI->BeginCommandList(pList);

        PipelineBarrier barrier = {
            .m_CurrentUsage = LR_RESOURCE_USAGE_SHADER_RESOURCE,
            .m_CurrentStage = LR_PIPELINE_STAGE_PIXEL_SHADER,
            .m_CurrentAccess = LR_PIPELINE_ACCESS_SHADER_READ,
            .m_NextUsage = LR_RESOURCE_USAGE_TRANSFER_DST,
            .m_NextStage = LR_PIPELINE_STAGE_HOST,
            .m_NextAccess = LR_PIPELINE_ACCESS_HOST_READ,
        };
        pList->SetImageBarrier(m_pFontImage, &barrier);

        pList->CopyBuffer(pImageBuffer, m_pFontImage);

        barrier = {
            .m_CurrentUsage = LR_RESOURCE_USAGE_TRANSFER_DST,
            .m_CurrentStage = LR_PIPELINE_STAGE_HOST,
            .m_CurrentAccess = LR_PIPELINE_ACCESS_HOST_READ,
            .m_NextUsage = LR_RESOURCE_USAGE_SHADER_RESOURCE,
            .m_NextStage = LR_PIPELINE_STAGE_PIXEL_SHADER,
            .m_NextAccess = LR_PIPELINE_ACCESS_SHADER_READ,
        };
        pList->SetImageBarrier(m_pFontImage, &barrier);

        pAPI->EndCommandList(pList);
        pAPI->ExecuteCommandList(pList, true);

        pAPI->DeleteBuffer(pImageBuffer);

        /// CREATE PIPELINE

        SamplerDesc samplerDesc = {};
        samplerDesc.m_MinFilter = LR_FILTERING_LINEAR;
        samplerDesc.m_MagFilter = LR_FILTERING_LINEAR;
        samplerDesc.m_MipFilter = LR_FILTERING_NEAREST;
        samplerDesc.m_AddressU = LR_TEXTURE_ADDRESS_WRAP;
        samplerDesc.m_AddressV = LR_TEXTURE_ADDRESS_WRAP;
        samplerDesc.m_MaxLOD = 0;

        Sampler *pSampler = pAPI->CreateSampler(&samplerDesc);

        BufferReadStream vertexShaderData((void *)kShader_imgui_v, kShader_imgui_v_length);
        BufferReadStream pixelShaderData((void *)kShader_imgui_p, kShader_imgui_p_length);
        Shader *pVertexShader = pAPI->CreateShader(LR_SHADER_STAGE_VERTEX, vertexShaderData);
        Shader *pPixelShader = pAPI->CreateShader(LR_SHADER_STAGE_PIXEL, pixelShaderData);

        DescriptorSetDesc descriptorDesc = {};
        descriptorDesc.m_BindingCount = 1;

        descriptorDesc.m_pBindings[0] = {
            .m_BindingID = 0,
            .m_Type = LR_DESCRIPTOR_TYPE_SHADER_RESOURCE,
            .m_TargetShader = LR_SHADER_STAGE_PIXEL,
            .m_ArraySize = 1,
            .m_pImage = m_pFontImage,
        };

        m_pDescriptorSet = pAPI->CreateDescriptorSet(&descriptorDesc);
        pAPI->UpdateDescriptorData(m_pDescriptorSet, &descriptorDesc);

        descriptorDesc.m_pBindings[0] = {
            .m_BindingID = 0,
            .m_Type = LR_DESCRIPTOR_TYPE_SAMPLER,
            .m_TargetShader = LR_SHADER_STAGE_PIXEL,
            .m_ArraySize = 1,
            .m_pSampler = pSampler,
        };

        m_pSamplerSet = pAPI->CreateDescriptorSet(&descriptorDesc);
        pAPI->UpdateDescriptorData(m_pSamplerSet, &descriptorDesc);

        InputLayout inputLayout = {
            { VertexAttribType::Vec2, "POSITION" },
            { VertexAttribType::Vec2, "TEXCOORD" },
            { VertexAttribType::Vec4U_Packed, "COLOR" },
        };

        PipelineAttachment colorAttachment = {
            .m_Format = pAPI->GetSwapChain()->m_ImageFormat,
            .m_BlendEnable = true,
            .m_WriteMask = LR_COLOR_MASK_R | LR_COLOR_MASK_G | LR_COLOR_MASK_B | LR_COLOR_MASK_A,
            .m_SrcBlend = LR_BLEND_FACTOR_SRC_ALPHA,
            .m_DstBlend = LR_BLEND_FACTOR_INV_SRC_ALPHA,
            .m_SrcBlendAlpha = LR_BLEND_FACTOR_ONE,
            .m_DstBlendAlpha = LR_BLEND_FACTOR_SRC_ALPHA,
            .m_ColorBlendOp = LR_BLEND_OP_ADD,
            .m_AlphaBlendOp = LR_BLEND_OP_ADD,
        };

        PushConstantDesc cameraConstant = {
            .m_Stage = LR_SHADER_STAGE_VERTEX,
            .m_Offset = 0,
            .m_Size = sizeof(XMMATRIX),
        };

        GraphicsPipelineBuildInfo buildInfo = {
            .m_RenderTargetCount = 1,
            .m_pRenderTargets = { colorAttachment },
            .m_ShaderCount = 2,
            .m_ppShaders = { pVertexShader, pPixelShader },
            .m_DescriptorSetCount = 2,
            .m_ppDescriptorSets = { m_pDescriptorSet, m_pSamplerSet },
            .m_PushConstantCount = 1,
            .m_pPushConstants = { cameraConstant },
            .m_pInputLayout = &inputLayout,
            .m_SetFillMode = LR_FILL_MODE_FILL,
            .m_SetCullMode = LR_CULL_MODE_NONE,
            .m_EnableDepthWrite = false,
            .m_EnableDepthTest = false,
            .m_DepthCompareOp = LR_COMPARE_OP_NEVER,
            .m_MultiSampleBitCount = 1,
        };

        m_pPipeline = pAPI->CreateGraphicsPipeline(&buildInfo);
    }

    void ImGuiPass::Draw(BaseAPI *pAPI, XMMATRIX &mat2d)
    {
        ZoneScoped;

        // TODO: Actual UI pass with it's own render target
        Image *pImage = pAPI->GetSwapChain()->GetCurrentImage();

        ImDrawData *pDrawData = ImGui::GetDrawData();

        if (pDrawData->DisplaySize.x <= 0.0f || pDrawData->DisplaySize.y <= 0.0f)
            return;

        if (pDrawData->TotalIdxCount == 0)
            return;

        if (m_VertexCount < pDrawData->TotalVtxCount || m_IndexCount < pDrawData->TotalIdxCount)
        {
            BufferDesc bufferDesc = {};
            bufferDesc.m_TargetAllocator = LR_API_ALLOCATOR_BUFFER_FRAMETIME;

            // Vertex buffer

            m_VertexCount = pDrawData->TotalVtxCount + 1500;

            if (m_pVertexBuffer)
                pAPI->DeleteBuffer(m_pVertexBuffer);

            bufferDesc.m_UsageFlags = LR_RESOURCE_USAGE_VERTEX_BUFFER | LR_RESOURCE_USAGE_HOST_VISIBLE;
            bufferDesc.m_Stride = sizeof(ImDrawVert);
            bufferDesc.m_DataLen = m_VertexCount * sizeof(ImDrawVert);

            m_pVertexBuffer = pAPI->CreateBuffer(&bufferDesc);

            // Index buffer

            m_IndexCount = pDrawData->TotalIdxCount + 1500;

            if (m_pIndexBuffer)
                pAPI->DeleteBuffer(m_pIndexBuffer);

            bufferDesc.m_UsageFlags = LR_RESOURCE_USAGE_INDEX_BUFFER | LR_RESOURCE_USAGE_HOST_VISIBLE;
            bufferDesc.m_Stride = sizeof(ImDrawIdx);
            bufferDesc.m_DataLen = m_IndexCount * sizeof(ImDrawIdx);

            m_pIndexBuffer = pAPI->CreateBuffer(&bufferDesc);
        }

        // Map memory

        u32 mapDataOffset = 0;
        void *pMapData = nullptr;

        pAPI->MapMemory(m_pVertexBuffer, pMapData);
        for (u32 i = 0; i < pDrawData->CmdListsCount; i++)
        {
            const ImDrawList *pDrawList = pDrawData->CmdLists[i];
            memcpy(
                (ImDrawVert *)pMapData + mapDataOffset,
                pDrawList->VtxBuffer.Data,
                pDrawList->VtxBuffer.Size * sizeof(ImDrawVert));

            mapDataOffset += pDrawList->VtxBuffer.Size;
        }

        pAPI->UnmapMemory(m_pVertexBuffer);
        mapDataOffset = 0;

        pAPI->MapMemory(m_pIndexBuffer, pMapData);
        for (u32 i = 0; i < pDrawData->CmdListsCount; i++)
        {
            const ImDrawList *pDrawList = pDrawData->CmdLists[i];
            memcpy(
                (ImDrawIdx *)pMapData + mapDataOffset,
                pDrawList->IdxBuffer.Data,
                pDrawList->IdxBuffer.Size * sizeof(ImDrawIdx));

            mapDataOffset += pDrawList->IdxBuffer.Size;
        }

        pAPI->UnmapMemory(m_pIndexBuffer);
        mapDataOffset = 0;

        CommandList *pList = pAPI->GetCommandList();
        pAPI->BeginCommandList(pList);

        CommandListAttachment attachment = {};
        attachment.m_pHandle = pImage;
        attachment.m_LoadOp = LR_ATTACHMENT_OP_CLEAR;
        attachment.m_StoreOp = LR_ATTACHMENT_OP_STORE;
        attachment.m_ClearVal = ClearValue({ 0.0, 0.0, 0.0, 1.0 });

        CommandListBeginDesc beginDesc = {};
        beginDesc.m_RenderArea = { 0, 0, (u32)pDrawData->DisplaySize.x, (u32)pDrawData->DisplaySize.y };
        beginDesc.m_ColorAttachmentCount = 1;
        beginDesc.m_pColorAttachments[0] = attachment;
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

                pList->SetScissors(0, clipMin.x, clipMin.y, clipMax.x - clipMin.x, clipMax.y - clipMin.y);

                DescriptorSet *pSet = (DescriptorSet *)cmd.TextureId;
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
        pAPI->ExecuteCommandList(pList, true);
    }

}  // namespace lr::Renderer