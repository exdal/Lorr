#include "Renderer/Pass.hh"

#include "Resources/imgui.v.hh"
#include "Resources/imgui.p.hh"

#include <imgui.h>

namespace lr::Renderer
{
    void AddImguiPass(RenderGraph *pGraph)
    {
        struct PushConstant
        {
            XMFLOAT2 m_Scale;
            XMFLOAT2 m_Translate;
        };

        struct ImguiData
        {
            Graphics::Image *m_pFontImage;
            Graphics::Sampler *m_pSampler;
            Graphics::DescriptorSet *m_pFontDescriptor;
            Graphics::DescriptorSet *m_pSamplerDescriptor;
            Graphics::Pipeline *m_pPipeline;
            Graphics::Buffer *m_pVertexBuffer;
            Graphics::Buffer *m_pIndexBuffer;
            u32 m_VertexCount;
            u32 m_IndexCount;
        };

        pGraph->AddGraphicsPass<ImguiData>(
            "imgui",
            [](RenderPassResourceManager &resourceMan, ImguiData &passData, Graphics::CommandList *pList)
            {
                auto &io = ImGui::GetIO();

                u8 *pFontData = nullptr;
                i32 fontW, fontH, bpp;
                io.Fonts->GetTexDataAsRGBA32(&pFontData, &fontW, &fontH, &bpp);

                Graphics::ImageDesc imageDesc = {
                    .m_UsageFlags = LR_RESOURCE_USAGE_SHADER_RESOURCE | LR_RESOURCE_USAGE_TRANSFER_DST,
                    .m_Format = LR_IMAGE_FORMAT_RGBA8F,
                    .m_TargetAllocator = LR_API_ALLOCATOR_IMAGE_TLSF,
                    .m_Width = (u32)fontW,
                    .m_Height = (u32)fontH,
                    .m_ArraySize = 1,
                    .m_MipMapLevels = 1,
                };

                passData.m_pFontImage =
                    resourceMan.CreateBufferedImage("imgui_font_image", &imageDesc, pFontData);

                Graphics::SamplerDesc samplerDesc = {};
                samplerDesc.m_MinFilter = LR_FILTERING_LINEAR;
                samplerDesc.m_MagFilter = LR_FILTERING_LINEAR;
                samplerDesc.m_MipFilter = LR_FILTERING_NEAREST;
                samplerDesc.m_AddressU = LR_TEXTURE_ADDRESS_WRAP;
                samplerDesc.m_AddressV = LR_TEXTURE_ADDRESS_WRAP;
                samplerDesc.m_MaxLOD = 0;

                passData.m_pSampler = resourceMan.CreateSampler("imgui_font_sampler", &samplerDesc);

                Graphics::DescriptorSetDesc descriptorDesc = {};
                descriptorDesc.m_BindingCount = 1;
                descriptorDesc.m_pBindings[0] = {
                    .m_BindingID = 0,
                    .m_Type = LR_DESCRIPTOR_TYPE_SHADER_RESOURCE_IMAGE,
                    .m_TargetShader = LR_SHADER_STAGE_PIXEL,
                    .m_ArraySize = 1,
                    .m_pImage = passData.m_pFontImage,
                };

                passData.m_pFontDescriptor =
                    resourceMan.CreateDescriptorSet("imgui_font_descriptor", &descriptorDesc);

                descriptorDesc.m_pBindings[0] = {
                    .m_BindingID = 0,
                    .m_Type = LR_DESCRIPTOR_TYPE_SAMPLER,
                    .m_TargetShader = LR_SHADER_STAGE_PIXEL,
                    .m_ArraySize = 1,
                    .m_pSampler = passData.m_pSampler,
                };

                passData.m_pSamplerDescriptor =
                    resourceMan.CreateDescriptorSet("imgui_sampler_descriptor", &descriptorDesc);

                Graphics::InputLayout inputLayout = {
                    { Graphics::VertexAttribType::Vec2, "POSITION" },
                    { Graphics::VertexAttribType::Vec2, "TEXCOORD" },
                    { Graphics::VertexAttribType::Vec4U_Packed, "COLOR" },
                };

                Graphics::PipelineAttachment colorAttachment = {
                    .m_Format = resourceMan.GetSwapChainFormat(),
                    .m_BlendEnable = true,
                    .m_WriteMask = LR_COLOR_MASK_R | LR_COLOR_MASK_G | LR_COLOR_MASK_B | LR_COLOR_MASK_A,
                    .m_SrcBlend = LR_BLEND_FACTOR_SRC_ALPHA,
                    .m_DstBlend = LR_BLEND_FACTOR_INV_SRC_ALPHA,
                    .m_SrcBlendAlpha = LR_BLEND_FACTOR_ONE,
                    .m_DstBlendAlpha = LR_BLEND_FACTOR_SRC_ALPHA,
                    .m_ColorBlendOp = LR_BLEND_OP_ADD,
                    .m_AlphaBlendOp = LR_BLEND_OP_ADD,
                };

                Graphics::PushConstantDesc cameraConstant = {
                    .m_Stage = LR_SHADER_STAGE_VERTEX,
                    .m_Offset = 0,
                    .m_Size = sizeof(PushConstant),
                };

                BufferReadStream vertexShaderData((void *)kShader_imgui_v, kShader_imgui_v_length);
                BufferReadStream pixelShaderData((void *)kShader_imgui_p, kShader_imgui_p_length);
                Graphics::Shader *pVertexShader =
                    resourceMan.CreateShader(LR_SHADER_STAGE_VERTEX, vertexShaderData);
                Graphics::Shader *pPixelShader =
                    resourceMan.CreateShader(LR_SHADER_STAGE_PIXEL, pixelShaderData);

                Graphics::GraphicsPipelineBuildInfo buildInfo = {
                    .m_RenderTargetCount = 1,
                    .m_pRenderTargets = { colorAttachment },
                    .m_ShaderCount = 2,
                    .m_ppShaders = { pVertexShader, pPixelShader },
                    .m_DescriptorSetCount = 2,
                    .m_ppDescriptorSets = { passData.m_pFontDescriptor, passData.m_pSamplerDescriptor },
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

                passData.m_pPipeline = resourceMan.CreateGraphicsPipeline("imgui_pipeline", buildInfo);

                resourceMan.DeleteShader(pVertexShader);
                resourceMan.DeleteShader(pPixelShader);
            },
            [=](RenderPassResourceManager &resourceMan, ImguiData &passData, Graphics::CommandList *pList)
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
                        resourceMan.DeleteBuffer("imgui_vertex_buffer");
                    if (passData.m_pIndexBuffer)
                        resourceMan.DeleteBuffer("imgui_index_buffer");

                    Graphics::BufferDesc bufferDesc = {};
                    bufferDesc.m_TargetAllocator = LR_API_ALLOCATOR_BUFFER_TLSF_HOST;

                    bufferDesc.m_UsageFlags = LR_RESOURCE_USAGE_VERTEX_BUFFER | LR_RESOURCE_USAGE_HOST_VISIBLE;
                    bufferDesc.m_Stride = sizeof(ImDrawVert);
                    bufferDesc.m_DataLen = passData.m_VertexCount * sizeof(ImDrawVert);
                    passData.m_pVertexBuffer = resourceMan.CreateBuffer("imgui_vertex_buffer", &bufferDesc);

                    bufferDesc.m_UsageFlags = LR_RESOURCE_USAGE_INDEX_BUFFER | LR_RESOURCE_USAGE_HOST_VISIBLE;
                    bufferDesc.m_Stride = sizeof(ImDrawIdx);
                    bufferDesc.m_DataLen = passData.m_IndexCount * sizeof(ImDrawIdx);
                    passData.m_pIndexBuffer = resourceMan.CreateBuffer("imgui_index_buffer", &bufferDesc);
                }

                u32 mapDataOffset = 0;
                void *pMapData = nullptr;

                resourceMan.MapBuffer(passData.m_pVertexBuffer, pMapData);
                for (u32 i = 0; i < pDrawData->CmdListsCount; i++)
                {
                    const ImDrawList *pDrawList = pDrawData->CmdLists[i];
                    memcpy(
                        (ImDrawVert *)pMapData + mapDataOffset,
                        pDrawList->VtxBuffer.Data,
                        pDrawList->VtxBuffer.Size * sizeof(ImDrawVert));

                    mapDataOffset += pDrawList->VtxBuffer.Size;
                }

                resourceMan.UnmapBuffer(passData.m_pVertexBuffer);
                mapDataOffset = 0;

                resourceMan.MapBuffer(passData.m_pIndexBuffer, pMapData);
                for (u32 i = 0; i < pDrawData->CmdListsCount; i++)
                {
                    const ImDrawList *pDrawList = pDrawData->CmdLists[i];
                    memcpy(
                        (ImDrawIdx *)pMapData + mapDataOffset,
                        pDrawList->IdxBuffer.Data,
                        pDrawList->IdxBuffer.Size * sizeof(ImDrawIdx));

                    mapDataOffset += pDrawList->IdxBuffer.Size;
                }

                resourceMan.UnmapBuffer(passData.m_pIndexBuffer);
                mapDataOffset = 0;

                CommandListAttachment attachment = {};
                attachment.m_pHandle = resourceMan.GetSwapChainImage();
                attachment.m_LoadOp = LR_ATTACHMENT_OP_CLEAR;
                attachment.m_StoreOp = LR_ATTACHMENT_OP_STORE;
                attachment.m_ClearVal = ClearValue({ 0.0, 0.0, 0.0, 1.0 });

                CommandListBeginDesc beginDesc = {};
                beginDesc.m_RenderArea = { 0, 0, (u32)pDrawData->DisplaySize.x, (u32)pDrawData->DisplaySize.y };
                beginDesc.m_ColorAttachmentCount = 1;
                beginDesc.m_pColorAttachments[0] = attachment;
                pList->BeginPass(&beginDesc);

                pList->SetGraphicsPipeline(passData.m_pPipeline);

                PushConstant pushConstantData = {};
                pushConstantData.m_Scale.x = 2.0 / pDrawData->DisplaySize.x;
                pushConstantData.m_Scale.y = -2.0 / pDrawData->DisplaySize.y;
                pushConstantData.m_Translate.x = -1.0 - pDrawData->DisplayPos.x * pushConstantData.m_Scale.x;
                pushConstantData.m_Translate.y = 1.0 - pDrawData->DisplayPos.y * pushConstantData.m_Scale.y;

                pList->SetGraphicsPushConstants(
                    passData.m_pPipeline, LR_SHADER_STAGE_VERTEX, &pushConstantData, sizeof(PushConstant));

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

                        pList->SetGraphicsDescriptorSets({ pSet, passData.m_pSamplerDescriptor });

                        pList->SetPrimitiveType(LR_PRIMITIVE_TYPE_TRIANGLE_LIST);
                        pList->DrawIndexed(
                            cmd.ElemCount, cmd.IdxOffset + indexOffset, cmd.VtxOffset + vertexOffset);
                    }

                    vertexOffset += pDrawList->VtxBuffer.Size;
                    indexOffset += pDrawList->IdxBuffer.Size;
                }

                pList->SetScissors(0, 0, 0, pDrawData->DisplaySize.x, pDrawData->DisplaySize.y);
                pList->EndPass();
            },
            [](RenderPassResourceManager &resourceMan)
            {
            });
    }

}  // namespace lr::Renderer