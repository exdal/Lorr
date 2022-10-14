#include "ImGuiHandler.hh"

#include "Engine.hh"

namespace lr::UI
{
    static Graphics::InputLayout g_ImGuiInputLayout = {
        { Graphics::VertexAttribType::Vec2, "POSITION" },
        { Graphics::VertexAttribType::Vec2, "TEXCOORD" },
        { Graphics::VertexAttribType::Vec4U_Packed, "COLOR" },
    };

    void ImGuiHandler::Init()
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
        i32 fontW, fontH;
        io.Fonts->GetTexDataAsRGBA32(&pFontData, &fontW, &fontH);

        Graphics::ImageDesc imageDesc = {};
        imageDesc.Type = Graphics::ResourceType::ColorAttachment;
        imageDesc.Format = Graphics::ResourceFormat::RGBA32F;
        imageDesc.Mappable = true;

        Graphics::ImageData imageData = {};
        imageData.DataLen = fontW * fontH * sizeof(i32);
        imageData.pData = pFontData;
        imageData.Width = fontW;
        imageData.Height = fontH;

        // pAPI->CreateImage(&m_Texture, &imageDesc, &imageData);
        // pAPI->AllocateImageMemory(&m_Texture, Graphics::AllocatorType::None);

        // void *pImageData;
        // pAPI->MapMemory(&m_Texture, pImageData);
        // memcpy(pImageData, pFontData, imageData.DataLen);
        // pAPI->UnmapMemory(&m_Texture);
        // pAPI->BindMemory(&m_Texture);

        // pAPI->CreateImageView(&m_Texture);
    }

    void ImGuiHandler::NewFrame()
    {
        ZoneScoped;

        Engine *pEngine = GetEngine();
        PlatformWindow &window = pEngine->m_Window;
        ImGuiViewport *pMainViewport = ImGui::GetMainViewport();
        ImGuiIO &io = ImGui::GetIO();

        io.DisplaySize = ImVec2((float)window.m_Width, (float)window.m_Height);

        ImGui::NewFrame();
    }

    void ImGuiHandler::Render()
    {
        ZoneScopedN("ImGuiHandler::Render");

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

            Graphics::VKBuffer vertexStaging;
            Graphics::VKBuffer indexStaging;
            
            m_VertexCount = pDrawData->TotalVtxCount + 1500;

            pAPI->DeleteBuffer(&m_VertexBuffer);

            bufferDesc.UsageFlags = Graphics::BufferUsage::Vertex | Graphics::BufferUsage::CopySrc;
            bufferData.DataLen = m_VertexCount * sizeof(ImDrawVert);

            pAPI->CreateBuffer(&vertexStaging, &bufferDesc, &bufferData);
            pAPI->AllocateBufferMemory(&vertexStaging, Graphics::AllocatorType::None);
            pAPI->BindMemory(&vertexStaging);

            // Index buffer

            m_IndexCount = pDrawData->TotalIdxCount + 1500;

            pAPI->DeleteBuffer(&m_IndexBuffer);

            bufferDesc.UsageFlags = Graphics::BufferUsage::Index | Graphics::BufferUsage::CopySrc;
            bufferData.DataLen = m_IndexCount * sizeof(ImDrawIdx);

            pAPI->CreateBuffer(&indexStaging, &bufferDesc, &bufferData);
            pAPI->AllocateBufferMemory(&indexStaging, Graphics::AllocatorType::None);
            pAPI->BindMemory(&indexStaging);

            // Map memory

            u32 vertexOffset = 0;
            u32 indexOffset = 0;

            void *pVertexMapData = nullptr;
            void *pIndexMapData = nullptr;

            pAPI->MapMemory(&vertexStaging, pVertexMapData);
            pAPI->MapMemory(&indexStaging, pIndexMapData);

            for (u32 i = 0; i < pDrawData->CmdListsCount; i++)
            {
                const ImDrawList *pDrawList = pDrawData->CmdLists[i];

                memcpy((ImDrawVert *)pVertexMapData + vertexOffset, pDrawList->VtxBuffer.Data, pDrawList->VtxBuffer.Size * sizeof(ImDrawVert));
                memcpy((ImDrawIdx *)pIndexMapData + indexOffset, pDrawList->IdxBuffer.Data, pDrawList->IdxBuffer.Size * sizeof(ImDrawIdx));

                vertexOffset += pDrawList->VtxBuffer.Size;
                indexOffset += pDrawList->IdxBuffer.Size;
            }

            pAPI->UnmapMemory(&vertexStaging);
            pAPI->UnmapMemory(&indexStaging);

            Graphics::VKCommandList *pList = pAPI->GetCommandList();
            pAPI->BeginCommandList(pList);

            pAPI->TransferBufferMemory(pList, &vertexStaging, &m_VertexBuffer, Graphics::AllocatorType::BufferTLSF);
            pAPI->TransferBufferMemory(pList, &indexStaging, &m_IndexBuffer, Graphics::AllocatorType::BufferTLSF);

            pAPI->EndCommandList(pList);
            pAPI->ExecuteCommandList(pList);

            pAPI->DeleteBuffer(&vertexStaging);
            pAPI->DeleteBuffer(&indexStaging);
        }

        Graphics::VKCommandList *pList = pAPI->GetCommandList();
        pAPI->BeginCommandList(pList);

        Graphics::CommandRenderPassBeginInfo beginInfo = {};
        beginInfo.RenderArea.z = window.m_Width;
        beginInfo.RenderArea.w = window.m_Height;
        beginInfo.ClearValueCount = 2;
        beginInfo.pClearValues[0] = Graphics::ClearValue({ 0.0, 0.0, 0.0, 1.0 });
        beginInfo.pClearValues[1] = Graphics::ClearValue(1.0, 0xff);

        pList->BeginRenderPass(beginInfo, stateMan.m_pUIPass);
        pList->SetPipeline(&stateMan.m_UIPipeline);
        pList->SetPipelineDescriptorSets({ &stateMan.m_Camera2DDescriptor });

        pList->SetVertexBuffer(&m_VertexBuffer);
        pList->SetIndexBuffer(&m_IndexBuffer, false);

        pList->SetViewport(0, pDrawData->DisplaySize.x, pDrawData->DisplaySize.y, 0.0, 1.0);

        u32 vertexOffset = 0;
        u32 indexOffset = 0;
        XMFLOAT2 clipOffset = { pDrawData->DisplayPos.x, pDrawData->DisplayPos.y };

        for (u32 i = 0; i < pDrawData->CmdListsCount; i++)
        {
            const ImDrawList *pDrawList = pDrawData->CmdLists[i];

            for (auto &cmd : pDrawList->CmdBuffer)
            {
                if (cmd.UserCallback)
                {
                    if (cmd.UserCallback == ImDrawCallback_ResetRenderState)
                    {
                        assert(!"Setup Render state!!!");
                    }
                    else
                    {
                        cmd.UserCallback(pDrawList, &cmd);
                    }
                }
                else
                {
                    XMUINT2 clipMin(cmd.ClipRect.x - clipOffset.x, cmd.ClipRect.y - clipOffset.y);
                    XMUINT2 clipMax(cmd.ClipRect.z - clipOffset.x, cmd.ClipRect.w - clipOffset.y);
                    if (clipMax.x <= clipMin.x || clipMax.y <= clipMin.y)
                        continue;

                    pList->SetScissor(0, clipMin.x, clipMin.y, clipMax.x, clipMax.y);

                    pList->DrawIndexed(cmd.ElemCount, cmd.IdxOffset + indexOffset, cmd.VtxOffset + vertexOffset);
                }

                vertexOffset += pDrawList->VtxBuffer.Size;
                indexOffset += pDrawList->IdxBuffer.Size;
            }
        }

        pList->SetScissor(0, 0, 0, window.m_Width, window.m_Height);

        pList->EndRenderPass();
        pAPI->EndCommandList(pList);
        pAPI->ExecuteCommandList(pList);
    }

    void ImGuiHandler::EndFrame()
    {
        ZoneScoped;

        ImGui::EndFrame();
        ImGui::Render();

        Render();
    }

    void ImGuiHandler::Deinit()
    {
        ZoneScoped;
    }

}  // namespace lr::UI