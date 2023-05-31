// Created on Wednesday November 23rd 2022 by exdal
// Last modified on Thursday June 1st 2023 by exdal

#include "Graphics/Renderer/Pass.hh"

#include "Engine.hh"

#include "Graphics/Descriptor.hh"

#include <imgui.h>

namespace lr::Graphics
{
void AddImguiPass(RenderGraph *pGraph, eastl::string_view name)
{
    struct PushConstant
    {
        XMFLOAT2 m_Scale;
        XMFLOAT2 m_Translate;
        u32 m_StartIndex;
    };

    struct ImguiPassData
    {
        Image *m_pFontImage;
        Sampler *m_pSampler;
        Buffer *m_pVertexBuffer;
        Buffer *m_pIndexBuffer;
    };

    auto *pPass = pGraph->CreateGraphicsPassCb<ImguiPassData>(
        name,
        [pGraph](Context *pContext, ImguiPassData &data, RenderPassBuilder &builder)
        {
            ImGuiIO &io = ImGui::GetIO();

            u8 *pFontData = nullptr;
            i32 fontW, fontH, bpp;
            io.Fonts->GetTexDataAsRGBA32(&pFontData, &fontW, &fontH, &bpp);

            ImageDesc imageDesc = {
                .m_UsageFlags = ImageUsage::Sampled | ImageUsage::TransferDst,
                .m_Format = ImageFormat::RGBA8F,
                .m_TargetAllocator = ResourceAllocator::ImageTLSF,
                .m_Width = (u32)fontW,
                .m_Height = (u32)fontH,
                .m_ArraySize = 1,
                .m_MipMapLevels = 1,
            };
            data.m_pFontImage = pGraph->CreateImage("imgui_font", imageDesc);
            pGraph->FillImageData(data.m_pFontImage, eastl::span<u8>(pFontData, data.m_pFontImage->m_DataLen));
            free(pFontData);

            SamplerDesc samplerDesc = {
                .m_MinFilter = Filtering::Linear,
                .m_MagFilter = Filtering::Linear,
                .m_MipFilter = Filtering::Nearest,
                .m_AddressU = TextureAddressMode::Wrap,
                .m_AddressV = TextureAddressMode::Wrap,
                .m_MaxLOD = 0,
            };
            data.m_pSampler = pContext->CreateSampler(&samplerDesc);

            // just change `ImDrawVert` to `... struct alignas(8) ImDrawVert...`
            static_assert(eastl::alignment_of<ImDrawVert>() == 8, "ImDrawVert must be aligned to 8!");
            BufferDesc vertexBufferDesc = {
                .m_UsageFlags = BufferUsage::Storage,
                .m_TargetAllocator = ResourceAllocator::BufferTLSF_Host,
                .m_Stride = sizeof(ImDrawVert),
                .m_DataLen = sizeof(ImDrawVert) * 0xffff,
            };
            data.m_pVertexBuffer = pContext->CreateBuffer(&vertexBufferDesc);

            BufferDesc indexBufferDesc = {
                .m_UsageFlags = BufferUsage::Storage,
                .m_TargetAllocator = ResourceAllocator::BufferTLSF_Host,
                .m_Stride = sizeof(ImDrawIdx),
                .m_DataLen = sizeof(ImDrawIdx) * 0xffff,
            };
            data.m_pIndexBuffer = pContext->CreateBuffer(&indexBufferDesc);

            DescriptorGetInfo imageDescriptorInfo(data.m_pFontImage);
            DescriptorGetInfo samplerDescriptorInfo(data.m_pSampler);
            DescriptorGetInfo pBufferInfos[] = { data.m_pVertexBuffer, data.m_pIndexBuffer };
            builder.SetDescriptor(DescriptorType::SampledImage, imageDescriptorInfo);
            builder.SetDescriptor(DescriptorType::Sampler, samplerDescriptorInfo);
            builder.SetDescriptor(DescriptorType::StorageBuffer, pBufferInfos);

            auto *pVertexShaderData =
                Engine::GetResourceMan()->Get<Resource::ShaderResource>("shader://imgui.vs");
            auto *pPixelShaderData =
                Engine::GetResourceMan()->Get<Resource::ShaderResource>("shader://imgui.fs");

            Shader *pVertexShader = pContext->CreateShader(pVertexShaderData);
            Shader *pPixelShader = pContext->CreateShader(pPixelShaderData);

            builder.SetColorAttachment("$backbuffer", { AttachmentOp::Load, AttachmentOp::Store });
            builder.SetBlendAttachment({ true });
            builder.SetShader(pVertexShader);
            builder.SetShader(pPixelShader);
        },
        [](Context *pContext, ImguiPassData &passData, CommandList *pList)
        {
            ImDrawData *pDrawData = ImGui::GetDrawData();
            if (pDrawData->DisplaySize.x <= 0.0f || pDrawData->DisplaySize.y <= 0.0f)
                return;

            if (pDrawData->TotalIdxCount == 0)
                return;

            u32 mapDataOffset = 0;
            void *pMapData = nullptr;
            pContext->MapMemory(passData.m_pVertexBuffer, pMapData, 0, pDrawData->TotalVtxCount);
            for (u32 i = 0; i < pDrawData->CmdListsCount; i++)
            {
                const ImDrawList *pDrawList = pDrawData->CmdLists[i];
                memcpy(
                    (ImDrawVert *)pMapData + mapDataOffset,
                    pDrawList->VtxBuffer.Data,
                    pDrawList->VtxBuffer.Size * sizeof(ImDrawVert));

                mapDataOffset += pDrawList->VtxBuffer.Size;
            }
            pContext->UnmapMemory(passData.m_pVertexBuffer);

            mapDataOffset = 0;
            pContext->MapMemory(passData.m_pIndexBuffer, pMapData, 0, pDrawData->TotalIdxCount);
            for (u32 i = 0; i < pDrawData->CmdListsCount; i++)
            {
                const ImDrawList *pDrawList = pDrawData->CmdLists[i];
                memcpy(
                    (ImDrawIdx *)pMapData + mapDataOffset,
                    pDrawList->IdxBuffer.Data,
                    pDrawList->IdxBuffer.Size * sizeof(ImDrawIdx));

                mapDataOffset += pDrawList->IdxBuffer.Size;
            }
            pContext->UnmapMemory(passData.m_pIndexBuffer);

            PushConstant pushConstantData = {};
            pushConstantData.m_Scale.x = 2.0 / pDrawData->DisplaySize.x;
            pushConstantData.m_Scale.y = 2.0 / pDrawData->DisplaySize.y;
            pushConstantData.m_Translate.x = -1.0 - pDrawData->DisplayPos.x * pushConstantData.m_Scale.x;
            pushConstantData.m_Translate.y = -1.0 - pDrawData->DisplayPos.y * pushConstantData.m_Scale.y;

            pList->SetViewport(0, 0, 0, pDrawData->DisplaySize.x, pDrawData->DisplaySize.y);

            XMFLOAT2 clipOffset = { pDrawData->DisplayPos.x, pDrawData->DisplayPos.y };
            XMFLOAT2 clipScale = { pDrawData->FramebufferScale.x, pDrawData->FramebufferScale.y };

            u32 vertexOffset = 0;
            u32 indexOffset = 0;
            for (u32 i = 0; i < pDrawData->CmdListsCount; i++)
            {
                ImDrawList *pDrawList = pDrawData->CmdLists[i];
                for (ImDrawCmd &cmd : pDrawList->CmdBuffer)
                {
                    ImVec2 clipMin(cmd.ClipRect.x - clipOffset.x, cmd.ClipRect.y - clipOffset.y);
                    ImVec2 clipMax(cmd.ClipRect.z - clipOffset.x, cmd.ClipRect.w - clipOffset.y);
                    if (clipMax.x <= clipMin.x || clipMax.y <= clipMin.y)
                        continue;

                    pList->SetScissors(0, clipMin.x, clipMin.y, clipMax.x, clipMax.y);

                    Image *pImage = (Image *)cmd.TextureId;
                    if (!pImage)
                        pImage = passData.m_pFontImage;

                    BindlessLayout::Binding pBindings[] = {
                        { 0, passData.m_pVertexBuffer },
                        { 1, passData.m_pIndexBuffer },
                        { 2, pImage },
                        { 3, passData.m_pSampler },
                    };

                    BindlessLayout bindlessLayout(pBindings);
                    pList->SetBindlessLayout(bindlessLayout);

                    pushConstantData.m_StartIndex = cmd.IdxOffset + indexOffset;
                    pList->SetPushConstants(&pushConstantData, sizeof(PushConstant));
                    pList->SetPrimitiveType(PrimitiveType::TriangleList);
                    pList->Draw(cmd.ElemCount, cmd.VtxOffset + vertexOffset);
                }

                vertexOffset += pDrawList->VtxBuffer.Size;
                indexOffset += pDrawList->IdxBuffer.Size;
            }

            pList->SetScissors(0, 0, 0, pDrawData->DisplaySize.x, pDrawData->DisplaySize.y);
        },
        [](Context *pContext)
        {
        });
}

}  // namespace lr::Graphics