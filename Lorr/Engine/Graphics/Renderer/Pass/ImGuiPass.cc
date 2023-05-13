#include "Graphics/Descriptor.hh"
#include "Graphics/Renderer/Pass.hh"

#include <imgui.h>

namespace lr::Graphics
{
void AddImguiPass(RenderGraph *pGraph, eastl::string_view name)
{
    struct PushConstant
    {
        XMFLOAT2 m_Scale;
        XMFLOAT2 m_Translate;
    };

    struct ImguiPassData
    {
        Image *m_pFontImage;
        Sampler *m_pSampler;
        Buffer *m_pVertexBuffer;
        Buffer *m_pIndexBuffer;
        u32 m_VertexCount;
        u32 m_IndexCount;
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

            // pGraph->UploadImageData(
            //     data.m_pFontImage, MemoryAccess::SampledRead, pFontData, data.m_pFontImage->m_DataLen);
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

            DescriptorGetInfo imageDescriptorInfo(data.m_pFontImage);
            DescriptorGetInfo samplerDescriptorInfo(data.m_pSampler);

            builder.SetDescriptorSet(DescriptorType::SampledImage, imageDescriptorInfo);
            builder.SetDescriptorSet(DescriptorType::Sampler, samplerDescriptorInfo);

            FileStream vertexShaderData("imgui.v.glsl", false);
            FileStream pixelShaderData("imgui.p.glsl", false);

            builder.SetColorAttachment("$backbuffer", { AttachmentOp::Load, AttachmentOp::Store });
            builder.SetBlendAttachment({ true });
            // builder.SetShader(pContext->CreateShader(ShaderStage::Vertex, vertexShaderData));
            // builder.SetShader(pContext->CreateShader(ShaderStage::Pixel, pixelShaderData));
            builder.SetInputLayout({
                LR_VERTEX_ATTRIB_SFLOAT2,
                LR_VERTEX_ATTRIB_SFLOAT2,
                LR_VERTEX_ATTRIB_UINT_4N,
            });
        },
        [](Context *pContext, ImguiPassData &passData, CommandList *pList)
        {
            ImDrawData *pDrawData = ImGui::GetDrawData();
            if (pDrawData->DisplaySize.x <= 0.0f || pDrawData->DisplaySize.y <= 0.0f)
                return;

            if (pDrawData->TotalIdxCount == 0)
                return;

            u64 vertexBufferSize = 0;
            u64 indexBufferSize = 0;

            if (passData.m_VertexCount < pDrawData->TotalVtxCount
                || passData.m_IndexCount < pDrawData->TotalIdxCount)
            {
                passData.m_VertexCount = pDrawData->TotalVtxCount + 1500;
                passData.m_IndexCount = pDrawData->TotalIdxCount + 1500;

                if (passData.m_pVertexBuffer)
                    pContext->DeleteBuffer(passData.m_pVertexBuffer);
                if (passData.m_pIndexBuffer)
                    pContext->DeleteBuffer(passData.m_pIndexBuffer);

                vertexBufferSize = passData.m_VertexCount * sizeof(ImDrawVert);
                indexBufferSize = passData.m_IndexCount * sizeof(ImDrawIdx);

                BufferDesc bufferDesc = {};
                bufferDesc.m_TargetAllocator = ResourceAllocator::BufferTLSF_Host;

                bufferDesc.m_UsageFlags = BufferUsage::Vertex;
                bufferDesc.m_Stride = sizeof(ImDrawVert);
                bufferDesc.m_DataLen = vertexBufferSize;
                passData.m_pVertexBuffer = pContext->CreateBuffer(&bufferDesc);

                bufferDesc.m_UsageFlags = BufferUsage::Index;
                bufferDesc.m_Stride = sizeof(ImDrawIdx);
                bufferDesc.m_DataLen = indexBufferSize;
                passData.m_pIndexBuffer = pContext->CreateBuffer(&bufferDesc);

                u32 mapDataOffset = 0;
                void *pMapData = nullptr;

                pContext->MapMemory(passData.m_pVertexBuffer, pMapData, 0, vertexBufferSize);
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

                pContext->MapMemory(passData.m_pIndexBuffer, pMapData, 0, indexBufferSize);
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
                mapDataOffset = 0;
            }

            PushConstant pushConstantData = {};
            pushConstantData.m_Scale.x = 2.0 / pDrawData->DisplaySize.x;
            pushConstantData.m_Scale.y = 2.0 / pDrawData->DisplaySize.y;
            pushConstantData.m_Translate.x = -1.0 - pDrawData->DisplayPos.x * pushConstantData.m_Scale.x;
            pushConstantData.m_Translate.y = -1.0 - pDrawData->DisplayPos.y * pushConstantData.m_Scale.y;

            pList->SetPushConstants(ShaderStage::Vertex, 0, &pushConstantData, sizeof(PushConstant));

            u32 pBufferIndices[] = { 0, 1 };
            u64 pBufferOffsets[] = { 0, 0 };  // binding_count * set * size
            pList->SetDescriptorBufferOffsets(0, 2, pBufferIndices, pBufferOffsets);

            pList->SetVertexBuffer(passData.m_pVertexBuffer);
            pList->SetIndexBuffer(passData.m_pIndexBuffer, false);
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

                    // DescriptorSet *pSet = (DescriptorSet *)cmd.TextureId;
                    // if (!pSet)
                    //     pSet = passData.m_pFontDescriptor;

                    // pList->SetDescriptorSets({ pSet, passData.m_pSamplerDescriptor });
                    pList->SetPrimitiveType(PrimitiveType::TriangleList);
                    pList->DrawIndexed(
                        cmd.ElemCount, cmd.IdxOffset + indexOffset, cmd.VtxOffset + vertexOffset);
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