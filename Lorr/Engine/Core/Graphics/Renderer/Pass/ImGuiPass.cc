#include "Core/Graphics/Renderer/Pass.hh"

#include "Resources/imgui.v.hh"
#include "Resources/imgui.p.hh"

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
        DescriptorSet *m_pFontDescriptor;
        DescriptorSet *m_pSamplerDescriptor;
        Buffer *m_pVertexBuffer;
        Buffer *m_pIndexBuffer;
        u32 m_VertexCount;
        u32 m_IndexCount;
    };

    auto *pPass = pGraph->CreateGraphicsPassCb<ImguiPassData>(
        name,
        [pGraph](VKContext *pContext, ImguiPassData &data, GraphicsRenderPass &pass)
        {
            ImGuiIO &io = ImGui::GetIO();

            u8 *pFontData = nullptr;
            i32 fontW, fontH, bpp;
            io.Fonts->GetTexDataAsRGBA32(&pFontData, &fontW, &fontH, &bpp);

            ImageDesc imageDesc = {
                .m_UsageFlags = LR_IMAGE_USAGE_SAMPLED | LR_IMAGE_USAGE_TRANSFER_DST,
                .m_Format = LR_IMAGE_FORMAT_RGBA8F,
                .m_TargetAllocator = LR_API_ALLOCATOR_IMAGE_TLSF,
                .m_Width = (u32)fontW,
                .m_Height = (u32)fontH,
                .m_ArraySize = 1,
                .m_MipMapLevels = 1,
            };
            data.m_pFontImage = pGraph->CreateImage("imgui_font", imageDesc);

            pGraph->UploadImageData(
                data.m_pFontImage, LR_MEMORY_ACCESS_SHADER_READ, pFontData, data.m_pFontImage->m_DataLen);
            free(pFontData);

            SamplerDesc samplerDesc = {
                .m_MinFilter = LR_FILTERING_LINEAR,
                .m_MagFilter = LR_FILTERING_LINEAR,
                .m_MipFilter = LR_FILTERING_NEAREST,
                .m_AddressU = LR_TEXTURE_ADDRESS_WRAP,
                .m_AddressV = LR_TEXTURE_ADDRESS_WRAP,
                .m_MaxLOD = 0,
            };
            data.m_pSampler = pContext->CreateSampler(&samplerDesc);

            DescriptorLayoutElement fontDescriptorLayout = {
                .m_Type = LR_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                .m_TargetShader = LR_SHADER_STAGE_PIXEL,
            };

            DescriptorLayoutElement samplerDescriptorLayout = {
                .m_Type = LR_DESCRIPTOR_TYPE_SAMPLER,
                .m_TargetShader = LR_SHADER_STAGE_PIXEL,
            };

            DescriptorSetLayout *pFontLayout = pContext->CreateDescriptorSetLayout(fontDescriptorLayout);
            DescriptorSetLayout *pSamplerLayout = pContext->CreateDescriptorSetLayout(samplerDescriptorLayout);
            data.m_pFontDescriptor = pContext->CreateDescriptorSet(pFontLayout);
            data.m_pSamplerDescriptor = pContext->CreateDescriptorSet(pSamplerLayout);

            DescriptorWriteData fontDescriptorData(0, 1, data.m_pFontImage, LR_IMAGE_LAYOUT_READ_ONLY);
            DescriptorWriteData samplerDescriptorData(0, 1, data.m_pSampler);
            pContext->UpdateDescriptorSet(data.m_pFontDescriptor, fontDescriptorData);
            pContext->UpdateDescriptorSet(data.m_pSamplerDescriptor, samplerDescriptorData);

            BufferReadStream vertexShaderData((void *)kShader_imgui_v, kShader_imgui_v_length);
            BufferReadStream pixelShaderData((void *)kShader_imgui_p, kShader_imgui_p_length);

            pass.SetColorAttachment("$backbuffer", LR_MEMORY_ACCESS_COLOR_ATTACHMENT_WRITE);
            pass.SetBlendAttachment({ .m_BlendEnable = true });
            pass.SetShader(pContext->CreateShader(LR_SHADER_STAGE_VERTEX, vertexShaderData));
            pass.SetShader(pContext->CreateShader(LR_SHADER_STAGE_PIXEL, pixelShaderData));
            pass.SetLayout(pFontLayout);
            pass.SetLayout(pSamplerLayout);
            pass.SetPushConstant({ .m_Stage = LR_SHADER_STAGE_VERTEX, .m_Size = sizeof(PushConstant) });
            pass.SetInputLayout({
                LR_VERTEX_ATTRIB_SFLOAT2,
                LR_VERTEX_ATTRIB_SFLOAT2,
                LR_VERTEX_ATTRIB_UINT_4N,
            });
        },
        [](VKContext *pContext, ImguiPassData &passData, CommandList *pList)
        {
            ImDrawData *pDrawData = ImGui::GetDrawData();
            if (pDrawData->DisplaySize.x <= 0.0f || pDrawData->DisplaySize.y <= 0.0f)
                return;

            if (pDrawData->TotalIdxCount == 0)
                return;

            if (passData.m_VertexCount < pDrawData->TotalVtxCount
                || passData.m_IndexCount < pDrawData->TotalIdxCount)
            {
                passData.m_VertexCount = pDrawData->TotalVtxCount + 1500;
                passData.m_IndexCount = pDrawData->TotalIdxCount + 1500;

                if (passData.m_pVertexBuffer)
                    pContext->DeleteBuffer(passData.m_pVertexBuffer);
                if (passData.m_pIndexBuffer)
                    pContext->DeleteBuffer(passData.m_pIndexBuffer);

                BufferDesc bufferDesc = {};
                bufferDesc.m_TargetAllocator = LR_API_ALLOCATOR_BUFFER_TLSF_HOST;

                bufferDesc.m_UsageFlags = LR_BUFFER_USAGE_VERTEX;
                bufferDesc.m_Stride = sizeof(ImDrawVert);
                bufferDesc.m_DataLen = passData.m_VertexCount * sizeof(ImDrawVert);
                passData.m_pVertexBuffer = pContext->CreateBuffer(&bufferDesc);

                bufferDesc.m_UsageFlags = LR_BUFFER_USAGE_INDEX;
                bufferDesc.m_Stride = sizeof(ImDrawIdx);
                bufferDesc.m_DataLen = passData.m_IndexCount * sizeof(ImDrawIdx);
                passData.m_pIndexBuffer = pContext->CreateBuffer(&bufferDesc);
            }

            u32 mapDataOffset = 0;
            void *pMapData = nullptr;

            pContext->MapMemory(passData.m_pVertexBuffer, pMapData);
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

            pContext->MapMemory(passData.m_pIndexBuffer, pMapData);
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

            PushConstant pushConstantData = {};
            pushConstantData.m_Scale.x = 2.0 / pDrawData->DisplaySize.x;
            pushConstantData.m_Scale.y = 2.0 / pDrawData->DisplaySize.y;
            pushConstantData.m_Translate.x = -1.0 - pDrawData->DisplayPos.x * pushConstantData.m_Scale.x;
            pushConstantData.m_Translate.y = -1.0 - pDrawData->DisplayPos.y * pushConstantData.m_Scale.y;

            pList->SetPushConstants(LR_SHADER_STAGE_VERTEX, 0, &pushConstantData, sizeof(PushConstant));
            pList->SetVertexBuffer(passData.m_pVertexBuffer);
            pList->SetIndexBuffer(passData.m_pIndexBuffer, false);
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
                    ImVec2 clipMin(cmd.ClipRect.x - clipOffset.x, cmd.ClipRect.y - clipOffset.y);
                    ImVec2 clipMax(cmd.ClipRect.z - clipOffset.x, cmd.ClipRect.w - clipOffset.y);
                    if (clipMax.x <= clipMin.x || clipMax.y <= clipMin.y)
                        continue;

                    pList->SetScissors(0, clipMin.x, clipMin.y, clipMax.x, clipMax.y);

                    DescriptorSet *pSet = (DescriptorSet *)cmd.TextureId;
                    if (!pSet)
                        pSet = passData.m_pFontDescriptor;

                    pList->SetDescriptorSets({ pSet, passData.m_pSamplerDescriptor });
                    pList->SetPrimitiveType(LR_PRIMITIVE_TYPE_TRIANGLE_LIST);
                    pList->DrawIndexed(
                        cmd.ElemCount, cmd.IdxOffset + indexOffset, cmd.VtxOffset + vertexOffset);
                }

                vertexOffset += pDrawList->VtxBuffer.Size;
                indexOffset += pDrawList->IdxBuffer.Size;
            }

            pList->SetScissors(0, 0, 0, pDrawData->DisplaySize.x, pDrawData->DisplaySize.y);
        },
        [](VKContext *pContext)
        {
        });
}

}  // namespace lr::Graphics