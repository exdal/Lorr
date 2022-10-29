#include "ImGuiRenderer.hh"

#include "Engine.hh"

namespace lr::UI
{
    static Graphics::InputLayout g_ImGuiInputLayout = {
        { Graphics::VertexAttribType::Vec2, "POSITION" },
        { Graphics::VertexAttribType::Vec2, "TEXCOORD" },
        { Graphics::VertexAttribType::Vec4U_Packed, "COLOR" },
    };

    void ImGuiRenderer::Init()
    {
        ZoneScoped;

        Engine *pEngine = GetEngine();
        Graphics::VKAPI *pAPI = pEngine->m_pAPI;
        Graphics::APIStateManager &stateMan = pAPI->m_APIStateMan;

        /// INIT IMGUI ///

        ImGui::CreateContext();

        ImGuiIO &io = ImGui::GetIO();
        io.IniFilename = 0;

        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;      // Enable Docking

        io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;  // We can honor GetMouseCursor() values (optional)
        io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;   // We can honor io.WantSetMousePos requests (optional, rarely used)

        io.BackendPlatformName = "imgui_impl_lorr";

        ImGui::StyleColorsDark();

        u8 *pFontData;
        i32 fontW, fontH, bpp;
        io.Fonts->GetTexDataAsRGBA32(&pFontData, &fontW, &fontH, &bpp);

        Graphics::BufferDesc imageBufDesc = {};
        imageBufDesc.Mappable = true;
        imageBufDesc.UsageFlags = Graphics::BufferUsage::CopySrc;

        Graphics::BufferData imageBufData = {};
        imageBufData.DataLen = fontW * fontH * sizeof(i32);

        Graphics::VKBuffer imageBuffer = {};
        pAPI->CreateBuffer(&imageBuffer, &imageBufDesc, &imageBufData);
        pAPI->AllocateBufferMemory(&imageBuffer, Graphics::AllocatorType::None);
        pAPI->BindMemory(&imageBuffer);

        void *pMapData = nullptr;
        pAPI->MapMemory(&imageBuffer, pMapData);
        memcpy(pMapData, pFontData, imageBufData.DataLen);
        pAPI->UnmapMemory(&imageBuffer);

        Graphics::ImageDesc imageDesc = {};
        imageDesc.Format = Graphics::ResourceFormat::RGBA8F;
        imageDesc.Usage = Graphics::ImageUsage::Sampled | Graphics::ImageUsage::CopyDst;

        Graphics::ImageData imageData = {};
        imageData.DataLen = fontW * fontH * sizeof(i32);
        imageData.Width = fontW;
        imageData.Height = fontH;

        pAPI->CreateImage(&m_Texture, &imageDesc, &imageData);
        pAPI->AllocateImageMemory(&m_Texture, Graphics::AllocatorType::ImageTLSF);
        pAPI->BindMemory(&m_Texture);

        // Graphics::VKCommandList *pList = pAPI->GetCommandList();
        // pAPI->BeginCommandList(pList);

        // pList->CopyBuffer(&imageBuffer, &m_Texture);

        // pAPI->EndCommandList(pList);
        // pAPI->ExecuteCommandList(pList, true);

        // pAPI->CreateImageView(&m_Texture);
        // pAPI->CreateSampler(&m_Texture);

        // pAPI->DeleteBuffer(&imageBuffer);

        // Graphics::VKDescriptorSetDesc descriptorDesc = {};
        // descriptorDesc.BindingCount = 1;
        // descriptorDesc.pBindings[0].ArraySize = 1;
        // descriptorDesc.pBindings[0].Type = Graphics::DescriptorType::CombinedSampler;
        // descriptorDesc.pBindings[0].ShaderStageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        // descriptorDesc.pBindings[0].pImage = &m_Texture;
        // pAPI->UpdateDescriptorData(&stateMan.m_UISamplerDescriptor, &descriptorDesc);
    }

    void ImGuiRenderer::NewFrame()
    {
        ZoneScoped;

        Engine *pEngine = GetEngine();
        PlatformWindow &window = pEngine->m_Window;
        ImGuiViewport *pMainViewport = ImGui::GetMainViewport();
        ImGuiIO &io = ImGui::GetIO();

        io.DisplaySize = ImVec2((float)window.m_Width, (float)window.m_Height);

        ImGui::NewFrame();
    }

    void ImGuiRenderer::Render()
    {
        ZoneScopedN("ImGuiRenderer::Render");

        Engine *pEngine = GetEngine();

        PlatformWindow &window = pEngine->m_Window;
        Graphics::VKAPI *pAPI = pEngine->m_pAPI;
        Graphics::APIStateManager &stateMan = pAPI->m_APIStateMan;

        ImDrawData *pDrawData = ImGui::GetDrawData();

        if (pDrawData->DisplaySize.x <= 0.0f || pDrawData->DisplaySize.y <= 0.0f)
            return;

        if (pDrawData->TotalIdxCount == 0)
            return;

        Graphics::BufferDesc bufferDesc = {};
        Graphics::BufferData bufferData = {};

        bufferDesc.Mappable = true;

        if (m_VertexCount < pDrawData->TotalVtxCount || m_IndexCount < pDrawData->TotalIdxCount)
        {
            // Vertex buffer

            m_VertexCount = pDrawData->TotalVtxCount + 1500;

            pAPI->DeleteBuffer(&m_VertexBuffer);

            bufferDesc.UsageFlags = Graphics::BufferUsage::Vertex;
            bufferData.DataLen = m_VertexCount * sizeof(ImDrawVert);

            pAPI->CreateBuffer(&m_VertexBuffer, &bufferDesc, &bufferData);
            pAPI->AllocateBufferMemory(&m_VertexBuffer, Graphics::AllocatorType::None);
            pAPI->BindMemory(&m_VertexBuffer);

            // Index buffer

            m_IndexCount = pDrawData->TotalIdxCount + 1500;

            pAPI->DeleteBuffer(&m_IndexBuffer);

            bufferDesc.UsageFlags = Graphics::BufferUsage::Index;
            bufferData.DataLen = m_IndexCount * sizeof(ImDrawIdx);

            pAPI->CreateBuffer(&m_IndexBuffer, &bufferDesc, &bufferData);
            pAPI->AllocateBufferMemory(&m_IndexBuffer, Graphics::AllocatorType::None);
            pAPI->BindMemory(&m_IndexBuffer);
        }

        // Map memory

        u32 vertexOffset = 0;
        u32 indexOffset = 0;

        void *pVertexMapData = nullptr;
        void *pIndexMapData = nullptr;

        pAPI->MapMemory(&m_VertexBuffer, pVertexMapData);
        pAPI->MapMemory(&m_IndexBuffer, pIndexMapData);

        for (u32 i = 0; i < pDrawData->CmdListsCount; i++)
        {
            const ImDrawList *pDrawList = pDrawData->CmdLists[i];

            memcpy((ImDrawVert *)pVertexMapData + vertexOffset, pDrawList->VtxBuffer.Data, pDrawList->VtxBuffer.Size * sizeof(ImDrawVert));
            memcpy((ImDrawIdx *)pIndexMapData + indexOffset, pDrawList->IdxBuffer.Data, pDrawList->IdxBuffer.Size * sizeof(ImDrawIdx));

            vertexOffset += pDrawList->VtxBuffer.Size;
            indexOffset += pDrawList->IdxBuffer.Size;
        }

        pAPI->UnmapMemory(&m_VertexBuffer);
        pAPI->UnmapMemory(&m_IndexBuffer);

        // Graphics::VKCommandList *pList = pAPI->GetCommandList();
        // pAPI->BeginCommandList(pList);

        // Graphics::CommandRenderPassBeginInfo beginInfo = {};
        // beginInfo.RenderArea.z = window.m_Width;
        // beginInfo.RenderArea.w = window.m_Height;
        // beginInfo.ClearValueCount = 2;
        // beginInfo.pClearValues[0] = Graphics::ClearValue({ 0.0, 0.0, 0.0, 1.0 });
        // beginInfo.pClearValues[1] = Graphics::ClearValue(1.0, 0xff);

        // pList->BeginRenderPass(beginInfo, stateMan.m_pUIPass, nullptr);
        // pList->SetPipeline(&stateMan.m_UIPipeline);
        // pList->SetPipelineDescriptorSets({ &stateMan.m_Camera2DDescriptor, &stateMan.m_UISamplerDescriptor });

        // pList->SetVertexBuffer(&m_VertexBuffer);
        // pList->SetIndexBuffer(&m_IndexBuffer, false);

        // pList->SetViewport(0, pDrawData->DisplaySize.x, pDrawData->DisplaySize.y, 0.0, 1.0);

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

        //         pList->SetScissor(0, clipMin.x, clipMin.y, clipMax.x, clipMax.y);

        //         pList->DrawIndexed(cmd.ElemCount, cmd.IdxOffset, cmd.VtxOffset);
        //     }
        // }

        // pList->SetScissor(0, 0, 0, window.m_Width, window.m_Height);

        // pList->EndRenderPass();
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