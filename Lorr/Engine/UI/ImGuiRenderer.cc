#include "ImGuiRenderer.hh"

#include "Engine.hh"

namespace lr::UI
{
    static Graphics::InputLayout g_ImGuiInputLayout = {
        { Graphics::VertexAttribType::Vec2, "POSITION" },
        { Graphics::VertexAttribType::Vec2, "TEXCOORD" },
        { Graphics::VertexAttribType::Vec4U_Packed, "COLOR" },
    };

    void ImGuiRenderer::Init(u32 width, u32 height)
    {
        ZoneScoped;

        /// INIT IMGUI ///

        ImGui::CreateContext();

        ImGuiIO &io = ImGui::GetIO();
        io.IniFilename = 0;

        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;      // Enable Docking

        io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;  // We can honor GetMouseCursor() values (optional)
        io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;   // We can honor io.WantSetMousePos requests (optional, rarely used)

        io.BackendPlatformName = "imgui_impl_lorr";
        io.DisplaySize = ImVec2((float)width, (float)height);

        ImGui::StyleColorsDark();

        // u8 *pFontData;
        // i32 fontW, fontH, bpp;
        // io.Fonts->GetTexDataAsRGBA32(&pFontData, &fontW, &fontH, &bpp);

        // BufferDesc imageBufDesc = {};
        // imageBufDesc.Mappable = true;
        // imageBufDesc.UsageFlags = ResourceUsage::CopySrc;
        // imageBufDesc.TargetAllocator = AllocatorType::None;

        // BufferData imageBufData = {};
        // imageBufData.DataLen = fontW * fontH * sizeof(i32);

        // BaseBuffer *pImageBuffer = pAPI->CreateBuffer(&imageBufDesc, &imageBufData);

        // void *pMapData = nullptr;
        // pAPI->MapMemory(pImageBuffer, pMapData);
        // memcpy(pMapData, pFontData, imageBufData.DataLen);
        // pAPI->UnmapMemory(pImageBuffer);

        // ImageDesc imageDesc = {};
        // imageDesc.Format = ResourceFormat::RGBA8F;
        // imageDesc.Usage = ResourceUsage::ShaderResource | ResourceUsage::CopyDst;
        // imageDesc.TargetAllocator = AllocatorType::ImageTLSF;

        // ImageData imageData = {};
        // imageData.DataLen = imageBufData.DataLen;
        // imageData.Width = fontW;
        // imageData.Height = fontH;

        // m_pTexture = pAPI->CreateImage(&imageDesc, &imageData);

        // BaseCommandList *pList = pAPI->GetCommandList();
        // pAPI->BeginCommandList(pList);

        // pList->BarrierTransition(m_pTexture, ResourceUsage::Undefined, ShaderStage::None, ResourceUsage::CopyDst, ShaderStage::None, true);
        // pList->CopyBuffer(pImageBuffer, m_pTexture);
        // pList->BarrierTransition(m_pTexture, ResourceUsage::CopyDst, ShaderStage::None, ResourceUsage::ShaderResource, ShaderStage::Pixel, false);

        // pAPI->EndCommandList(pList);
        // pAPI->ExecuteCommandList(pList, true);

        // pAPI->CreateSampler(m_pTexture);

        // pAPI->DeleteBuffer(pImageBuffer);

        // DescriptorSetDesc descriptorDesc = {};
        // descriptorDesc.BindingCount = 1;
        // descriptorDesc.pBindings[0].ArraySize = 1;

        // descriptorDesc.pBindings[0].Type = DescriptorType::ConstantBufferView;
        // descriptorDesc.pBindings[0].TargetShader = ShaderStage::Vertex;
        // descriptorDesc.pBindings[0].pBuffer = m_pConstantBufferV;

        // m_pDescriptorSetV = pAPI->CreateDescriptorSet(&descriptorDesc);
        // pAPI->UpdateDescriptorData(m_pDescriptorSetV);

        // descriptorDesc.pBindings[0].Type = DescriptorType::ShaderResourceView;
        // descriptorDesc.pBindings[0].TargetShader = ShaderStage::Pixel;
        // descriptorDesc.pBindings[0].pImage = m_pTexture;

        // m_pDescriptorSetP = pAPI->CreateDescriptorSet(&descriptorDesc);
        // pAPI->UpdateDescriptorData(m_pDescriptorSetP);

        // InputLayout vertexLayout = {
        //     { Graphics::VertexAttribType::Vec2, "POSITION" },
        //     { Graphics::VertexAttribType::Vec2, "TEXCOORD" },
        //     { Graphics::VertexAttribType::Vec4U_Packed, "COLOR" },
        // };

        // BaseShader *pVertexShader = pAPI->CreateShader(ShaderStage::Vertex, "imgui.v.spv");
        // BaseShader *pPixelShader = pAPI->CreateShader(ShaderStage::Pixel, "imgui.p.spv");

        // GraphicsPipelineBuildInfo *pBuildInfo = pAPI->BeginPipelineBuildInfo();

        // pBuildInfo->SetInputLayout(vertexLayout);
        // pBuildInfo->SetShader(pVertexShader, "main");
        // pBuildInfo->SetShader(pPixelShader, "main");
        // pBuildInfo->SetFillMode(FillMode::Fill);
        // pBuildInfo->SetDepthState(false, false);
        // pBuildInfo->SetDepthFunction(DepthCompareOp::LessEqual);
        // pBuildInfo->SetCullMode(CullMode::None, false);
        // pBuildInfo->SetPrimitiveType(PrimitiveType::TriangleList);
        // pBuildInfo->SetDescriptorSets({ m_pDescriptorSetV, m_pDescriptorSetP });

        // PipelineAttachment colorAttachment = {};
        // colorAttachment.Format = ResourceFormat::RGBA8F;
        // colorAttachment.BlendEnable = true;
        // colorAttachment.WriteMask = 0xf;
        // colorAttachment.SrcBlend = BlendFactor::SrcAlpha;
        // colorAttachment.DstBlend = BlendFactor::InvSrcAlpha;
        // colorAttachment.SrcBlendAlpha = BlendFactor::One;
        // colorAttachment.DstBlendAlpha = BlendFactor::InvSrcAlpha;
        // colorAttachment.Blend = BlendOp::Add;
        // colorAttachment.BlendAlpha = BlendOp::Add;
        // pBuildInfo->AddAttachment(&colorAttachment, false);

        // m_pPipeline = pAPI->EndPipelineBuildInfo(pBuildInfo);

        // Camera2DDesc camera2DDesc;
        // camera2DDesc.Position = XMFLOAT2(0.0, 0.0);
        // camera2DDesc.ViewSize = XMFLOAT2(1.0, 1.0);
        // camera2DDesc.ZFar = 1.0;
        // camera2DDesc.ZNear = 0.0;

        // m_Camera2D.Init(&camera2DDesc);
    }

    void ImGuiRenderer::NewFrame()
    {
        ZoneScoped;

        // Engine *pEngine = GetEngine();
        // PlatformWindow &window = pEngine->m_Window;
        // ImGuiViewport *pMainViewport = ImGui::GetMainViewport();
        // ImGuiIO &io = ImGui::GetIO();

        // io.DisplaySize = ImVec2((float)window.m_Width, (float)window.m_Height);
        // m_Camera2D.SetSize({ (float)window.m_Width, (float)window.m_Height });
        // m_Camera2D.Update(0.0, 0.0);

        // ImGui::NewFrame();
    }

    void ImGuiRenderer::Render()
    {
        ZoneScopedN("ImGuiRenderer::Render");

        // using namespace Graphics;

        // Engine *pEngine = GetEngine();
        // PlatformWindow &window = pEngine->m_Window;
        // BaseAPI *pAPI = pEngine->m_pAPI;

        // ImDrawData *pDrawData = ImGui::GetDrawData();

        // if (pDrawData->DisplaySize.x <= 0.0f || pDrawData->DisplaySize.y <= 0.0f)
        //     return;

        // if (pDrawData->TotalIdxCount == 0)
        //     return;

        // XMMATRIX mat2D = XMMatrixMultiply(m_Camera2D.m_View, m_Camera2D.m_Projection);

        // void *pMapData = nullptr;
        // pAPI->MapMemory(m_pConstantBufferV, pMapData);
        // memcpy(pMapData, &mat2D, sizeof(XMMATRIX));
        // pAPI->UnmapMemory(m_pConstantBufferV);

        // if (m_VertexCount < pDrawData->TotalVtxCount || m_IndexCount < pDrawData->TotalIdxCount)
        // {
        //     Graphics::BufferDesc bufferDesc = {};
        //     Graphics::BufferData bufferData = {};

        //     bufferDesc.Mappable = true;
        //     bufferDesc.TargetAllocator = AllocatorType::None;

        //     // Vertex buffer

        //     m_VertexCount = pDrawData->TotalVtxCount + 1500;

        //     if (m_pVertexBuffer)
        //         pAPI->DeleteBuffer(m_pVertexBuffer);

        //     bufferDesc.UsageFlags = ResourceUsage::VertexBuffer;
        //     bufferData.DataLen = m_VertexCount * sizeof(ImDrawVert);

        //     m_pVertexBuffer = pAPI->CreateBuffer(&bufferDesc, &bufferData);

        //     // Index buffer

        //     m_IndexCount = pDrawData->TotalIdxCount + 1500;

        //     if (m_pIndexBuffer)
        //         pAPI->DeleteBuffer(m_pIndexBuffer);

        //     bufferDesc.UsageFlags = ResourceUsage::IndexBuffer;
        //     bufferData.DataLen = m_IndexCount * sizeof(ImDrawIdx);

        //     m_pIndexBuffer = pAPI->CreateBuffer(&bufferDesc, &bufferData);
        // }

        // // Map memory

        // u32 vertexOffset = 0;
        // u32 indexOffset = 0;

        // void *pVertexMapData = nullptr;
        // void *pIndexMapData = nullptr;

        // pAPI->MapMemory(m_pVertexBuffer, pVertexMapData);
        // pAPI->MapMemory(m_pIndexBuffer, pIndexMapData);

        // for (u32 i = 0; i < pDrawData->CmdListsCount; i++)
        // {
        //     const ImDrawList *pDrawList = pDrawData->CmdLists[i];

        //     memcpy((ImDrawVert *)pVertexMapData + vertexOffset, pDrawList->VtxBuffer.Data, pDrawList->VtxBuffer.Size * sizeof(ImDrawVert));
        //     memcpy((ImDrawIdx *)pIndexMapData + indexOffset, pDrawList->IdxBuffer.Data, pDrawList->IdxBuffer.Size * sizeof(ImDrawIdx));

        //     vertexOffset += pDrawList->VtxBuffer.Size;
        //     indexOffset += pDrawList->IdxBuffer.Size;
        // }

        // pAPI->UnmapMemory(m_pVertexBuffer);
        // pAPI->UnmapMemory(m_pIndexBuffer);

        // BaseCommandList *pList = pAPI->GetCommandList();
        // pAPI->BeginCommandList(pList);

        // CommandListAttachment attachment;
        // attachment.pHandle = pEngine->m_pAPI->GetSwapChain()->GetCurrentImage();
        // attachment.LoadOp = AttachmentOperation::Clear;
        // attachment.StoreOp = AttachmentOperation::Store;

        // CommandListBeginDesc beginDesc = {};
        // beginDesc.RenderArea = { 0, 0, (u32)pDrawData->DisplaySize.x, (u32)pDrawData->DisplaySize.y };
        // beginDesc.ColorAttachmentCount = 1;
        // beginDesc.pColorAttachments[0] = attachment;
        // pList->BeginPass(&beginDesc);

        // pList->SetPipeline(m_pPipeline);
        // pList->SetPipelineDescriptorSets({ m_pDescriptorSetV, m_pDescriptorSetP });

        // pList->SetVertexBuffer(m_pVertexBuffer);
        // pList->SetIndexBuffer(m_pIndexBuffer, false);

        // pList->SetViewport(0, 0, 0, pDrawData->DisplaySize.x, pDrawData->DisplaySize.y);

        // XMFLOAT2 clipOffset = { pDrawData->DisplayPos.x, pDrawData->DisplayPos.y };

        // for (u32 i = 0; i < pDrawData->CmdListsCount; i++)
        // {
        //     const ImDrawList *pDrawList = pDrawData->CmdLists[i];

        //     for (auto &cmd : pDrawList->CmdBuffer)
        //     {
        //         XMUINT2 clipMin(cmd.ClipRect.x - clipOffset.x, cmd.ClipRect.y - clipOffset.y);
        //         XMUINT2 clipMax(cmd.ClipRect.z - clipOffset.x, cmd.ClipRect.w - clipOffset.y);
        //         if (clipMax.x <= clipMin.x || clipMax.y <= clipMin.y)
        //             continue;

        //         pList->SetScissors(0, clipMin.x, clipMin.y, clipMax.x, clipMax.y);
        //         pList->SetPrimitiveType(PrimitiveType::TriangleList);

        //         pList->DrawIndexed(cmd.ElemCount, cmd.IdxOffset, cmd.VtxOffset);
        //     }
        // }

        // pList->EndPass();

        // pAPI->EndCommandList(pList);
        // pAPI->ExecuteCommandList(pList, false);
    }

    void ImGuiRenderer::EndFrame()
    {
        ZoneScoped;

        ImGui::EndFrame();
        ImGui::Render();

        Render();
    }

    void ImGuiRenderer::Destroy()
    {
        ZoneScoped;
    }

}  // namespace lr::UI