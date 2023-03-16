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
        Pipeline *m_pPipeline;
        Image *m_pFontImage;
        Sampler *m_pSampler;
        DescriptorSet *m_pFontDescriptor;
        DescriptorSet *m_pSamplerDescriptor;
        Buffer *m_pVertexBuffer;
        Buffer *m_pIndexBuffer;
        u32 m_VertexCount;
        u32 m_IndexCount;
    };

    GraphicsRenderPass *pPass = pGraph->CreateGraphicsPass<ImguiPassData>(name);

    pPass->AddColorAttachment(
        "$backbuffer", LR_MEMORY_ACCESS_RENDER_TARGET_READ | LR_MEMORY_ACCESS_RENDER_TARGET_WRITE);

    DescriptorLayoutElement pFontLayoutElement[] = {
        {
            .m_BindingID = 0,
            .m_Type = LR_DESCRIPTOR_TYPE_SHADER_RESOURCE_IMAGE,
            .m_TargetShader = LR_SHADER_STAGE_PIXEL,
            .m_ArraySize = 1,
        },
    };
    DescriptorLayoutElement pSamplerLayoutElement[] = {
        {
            .m_BindingID = 0,
            .m_Type = LR_DESCRIPTOR_TYPE_SAMPLER,
            .m_TargetShader = LR_SHADER_STAGE_PIXEL,
            .m_ArraySize = 1,
        },
    };
    DescriptorLayoutElement *pPipelineElements[] = { pFontLayoutElement, pSamplerLayoutElement };

    pGraph->SetCallbacks<ImguiPassData>(
        pPass,
        [](RenderGraphResourceManager &resMan, ImguiPassData &data)
        {
            auto &io = ImGui::GetIO();

            u8 *pFontData = nullptr;
            i32 fontW, fontH, bpp;
            io.Fonts->GetTexDataAsRGBA32(&pFontData, &fontW, &fontH, &bpp);

            ImageDesc imageDesc = {
                .m_UsageFlags = LR_IMAGE_USAGE_SHADER_RESOURCE | LR_IMAGE_USAGE_TRANSFER_DST,
                .m_Format = LR_IMAGE_FORMAT_RGBA8F,
                .m_TargetAllocator = LR_API_ALLOCATOR_IMAGE_TLSF,
                .m_Width = (u32)fontW,
                .m_Height = (u32)fontH,
                .m_ArraySize = 1,
                .m_MipMapLevels = 1,
            };
            data.m_pFontImage = resMan.CreateImage("imgui_font", &imageDesc);

            SamplerDesc samplerDesc = {
                .m_MinFilter = LR_FILTERING_LINEAR,
                .m_MagFilter = LR_FILTERING_LINEAR,
                .m_MipFilter = LR_FILTERING_NEAREST,
                .m_AddressU = LR_TEXTURE_ADDRESS_WRAP,
                .m_AddressV = LR_TEXTURE_ADDRESS_WRAP,
                .m_MaxLOD = 0,
            };
            data.m_pSampler = resMan.CreateSampler("imgui_font_sampler", &samplerDesc);

            DescriptorSetLayout *pFontLayout = resMan.CreateDescriptorSetLayout(pFontLayoutElements);
            data.m_pFontDescriptor = resMan.CreateDescriptorSet(pFontLayout);

            DescriptorSetLayout *pSamplerLayout = resMan.CreateDescriptorSetLayout(pSamplerLayoutElements);
            data.m_pSamplerDescriptor = resMan.CreateDescriptorSet(pSamplerLayout);

            static InputLayout inputLayout = {
                LR_VERTEX_ATTRIB_SFLOAT2,
                LR_VERTEX_ATTRIB_SFLOAT2,
                LR_VERTEX_ATTRIB_UINT_4N,
            };

            BufferReadStream vertexShaderData((void *)kShader_imgui_v, kShader_imgui_v_length);
            BufferReadStream pixelShaderData((void *)kShader_imgui_p, kShader_imgui_p_length);

            Shader *pShaders[] = {
                resMan.CreateShader(LR_SHADER_STAGE_VERTEX, vertexShaderData),
                resMan.CreateShader(LR_SHADER_STAGE_PIXEL, pixelShaderData),
            };

            GraphicsPipelineBuildInfo pipelineInfo = {
                .m_Shaders = pShaders,
                .m_MultiSampleBitCount = 1,
            };

            pPipeline = resMan.CreatePipeline(pipelineInfo);
        },
        [](RenderGraphResourceManager &resMan, ImguiPassData &passData, CommandList *)
        {

        },
        [](RenderGraphResourceManager &resMan, ImguiPassData &passData, CommandList *pList)
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
                    resMan.DeleteBuffer("imgui_vertex_buffer");
                if (passData.m_pIndexBuffer)
                    resMan.DeleteBuffer("imgui_index_buffer");

                BufferDesc bufferDesc = {};
                bufferDesc.m_TargetAllocator = LR_API_ALLOCATOR_BUFFER_TLSF_HOST;

                bufferDesc.m_UsageFlags = LR_BUFFER_USAGE_VERTEX_BUFFER;
                bufferDesc.m_Stride = sizeof(ImDrawVert);
                bufferDesc.m_DataLen = passData.m_VertexCount * sizeof(ImDrawVert);
                passData.m_pVertexBuffer = resMan.CreateBuffer("imgui_vertex_buffer", &bufferDesc);

                bufferDesc.m_UsageFlags = LR_BUFFER_USAGE_INDEX_BUFFER;
                bufferDesc.m_Stride = sizeof(ImDrawIdx);
                bufferDesc.m_DataLen = passData.m_IndexCount * sizeof(ImDrawIdx);
                passData.m_pIndexBuffer = resMan.CreateBuffer("imgui_index_buffer", &bufferDesc);
            }

            u32 mapDataOffset = 0;
            void *pMapData = nullptr;

            resMan.MapBuffer(passData.m_pVertexBuffer, pMapData);
            for (u32 i = 0; i < pDrawData->CmdListsCount; i++)
            {
                const ImDrawList *pDrawList = pDrawData->CmdLists[i];
                memcpy(
                    (ImDrawVert *)pMapData + mapDataOffset,
                    pDrawList->VtxBuffer.Data,
                    pDrawList->VtxBuffer.Size * sizeof(ImDrawVert));

                mapDataOffset += pDrawList->VtxBuffer.Size;
            }

            resMan.UnmapBuffer(passData.m_pVertexBuffer);
            mapDataOffset = 0;

            resMan.MapBuffer(passData.m_pIndexBuffer, pMapData);
            for (u32 i = 0; i < pDrawData->CmdListsCount; i++)
            {
                const ImDrawList *pDrawList = pDrawData->CmdLists[i];
                memcpy(
                    (ImDrawIdx *)pMapData + mapDataOffset,
                    pDrawList->IdxBuffer.Data,
                    pDrawList->IdxBuffer.Size * sizeof(ImDrawIdx));

                mapDataOffset += pDrawList->IdxBuffer.Size;
            }

            resMan.UnmapBuffer(passData.m_pIndexBuffer);
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
        [](RenderGraphResourceManager &)
        {
        });
}

}  // namespace lr::Graphics