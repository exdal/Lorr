#include "Engine.hh"

#include "Core/Graphics/Camera.hh"
#include "Core/IO/MemoryAllocator.hh"
#include "Core/Utils/Timer.hh"

#include "UI/ImGuiRenderer.hh"

#include "Core/Graphics/RHI/D3D12/D3D12API.hh"
#include "Core/Graphics/RHI/Vulkan/VKAPI.hh"

namespace lr
{
    using namespace Graphics;

    VKBuffer vertexBuffer;
    VKBuffer indexBuffer;

    UI::ImGuiRenderer imguiHandler;

    Memory::TLSFMemoryAllocator memoryAllocator;

    void Engine::Init(ApplicationDesc &desc, WindowDesc &windowDesc)
    {
        ZoneScoped;

        Logger::Init();

        m_Window.Init(windowDesc);

        m_pAPI = new D3D12API;
        m_pAPI->Init(&m_Window, windowDesc.Width, windowDesc.Height, APIFlags::VSync);

        Camera3DDesc camera3DDesc;
        camera3DDesc.Position = XMFLOAT3(0.0, 0.0, -3.0);
        camera3DDesc.ViewSize = XMFLOAT2(windowDesc.Width, windowDesc.Height);
        camera3DDesc.ViewDirection = XMFLOAT3(0.0, 0.0, 1.0);
        camera3DDesc.UpDirection = XMFLOAT3(0.0, 1.0, 0.0);
        camera3DDesc.FOV = 60.0;
        camera3DDesc.ZFar = 10000.0;
        camera3DDesc.ZNear = 0.1;

        m_Camera3D.Init(&camera3DDesc);

        Camera2DDesc camera2DDesc;
        camera2DDesc.Position = XMFLOAT2(0.0, 0.0);
        camera2DDesc.ViewSize = XMFLOAT2(windowDesc.Width, windowDesc.Height);
        camera2DDesc.ZFar = 1.0;
        camera2DDesc.ZNear = 0.0;

        m_Camera2D.Init(&camera2DDesc);

        // imguiHandler.Init();

        // XMFLOAT3 pData[3] = { { 0, -0.5, 0 }, { 0.5, 0.5, 0 }, { -0.5, 0.5, 0 } };
        // u32 pIndexData[3] = { 0, 1, 2 };

        // BufferDesc bufDesc = {};
        // bufDesc.UsageFlags = BufferUsage::Vertex | BufferUsage::CopySrc;
        // bufDesc.Mappable = true;

        // BufferData bufData = {};
        // bufData.DataLen = sizeof(XMFLOAT3) * 3;

        // VKBuffer tempVertexBuffer;
        // m_pAPI->CreateBuffer(&tempVertexBuffer, &bufDesc, &bufData);
        // m_pAPI->AllocateBufferMemory(&tempVertexBuffer, AllocatorType::None);

        // void *pMapData = nullptr;
        // m_pAPI->MapMemory(&tempVertexBuffer, pMapData);
        // memcpy(pMapData, pData, bufData.DataLen);
        // m_pAPI->UnmapMemory(&tempVertexBuffer);

        // m_pAPI->BindMemory(&tempVertexBuffer);

        // BaseCommandList *pList = m_pAPI->GetCommandList();

        // m_pAPI->BeginCommandList(pList);
        // // m_pAPI->TransferBufferMemory(pList, &tempVertexBuffer, &vertexBuffer, AllocatorType::BufferTLSF);

        // /// ------------------------------------------------------- ///

        // bufDesc.UsageFlags = BufferUsage::Index | BufferUsage::CopySrc;
        // bufDesc.Mappable = true;

        // bufData.DataLen = sizeof(u32) * 3;

        // VKBuffer tempIndexBuffer;
        // m_pAPI->CreateBuffer(&tempIndexBuffer, &bufDesc, &bufData);
        // m_pAPI->AllocateBufferMemory(&tempIndexBuffer, AllocatorType::None);

        // m_pAPI->MapMemory(&tempIndexBuffer, pMapData);
        // memcpy(pMapData, pIndexData, bufData.DataLen);
        // m_pAPI->UnmapMemory(&tempIndexBuffer);

        // m_pAPI->BindMemory(&tempIndexBuffer);

        // // m_pAPI->TransferBufferMemory(pList, &tempIndexBuffer, &indexBuffer, AllocatorType::BufferTLSF);

        // m_pAPI->EndCommandList(pList);
        // m_pAPI->ExecuteCommandList(pList, true);

        // m_pAPI->DeleteBuffer(&tempVertexBuffer);
        // m_pAPI->DeleteBuffer(&tempIndexBuffer);

        Run();
    }

    void Engine::OnWindowResize(u32 width, u32 height)
    {
        ZoneScoped;

        m_pAPI->ResizeSwapChain(width, height);
        m_Camera3D.SetSize(XMFLOAT2(width, height));
        m_Camera2D.SetSize(XMFLOAT2(width, height));
    }

    void Engine::Run()
    {
        ZoneScoped;

        Timer timer;

        while (!m_Window.ShouldClose())
        {
            // Graphics::APIStateManager &stateMan = m_pAPI->m_APIStateMan;

            f32 deltaTime = timer.elapsed();
            timer.reset();

            m_Camera3D.Update(0, 25.0);
            m_Camera2D.Update(0, 25.0);

            // XMMATRIX mat3D = XMMatrixMultiply(m_Camera3D.m_View, m_Camera3D.m_Projection);
            // XMMATRIX mat2D = XMMatrixMultiply(m_Camera2D.m_View, m_Camera2D.m_Projection);
            // stateMan.UpdateCamera3DData(mat3D);
            // stateMan.UpdateCamera2DData(mat2D);

            // VKCommandList *pList = m_pAPI->GetCommandList();
            // m_pAPI->BeginCommandList(pList);

            // CommandRenderPassBeginInfo beginInfo = {};
            // beginInfo.ClearValueCount = 2;
            // beginInfo.pClearValues[0] = ClearValue({ 0.0, 0.0, 0.0, 1.0 });
            // beginInfo.pClearValues[1] = ClearValue(1.0, 0xff);

            // pList->BeginRenderPass(beginInfo, stateMan.m_pGeometryPass, nullptr);
            // pList->SetPipeline(&stateMan.m_GeometryPipeline);
            // pList->SetPipelineDescriptorSets({ &stateMan.m_Camera3DDescriptor });

            // pList->SetViewport(0, m_Window.m_Width, m_Window.m_Height, 0.0, 1.0);
            // pList->SetScissor(0, 0, 0, m_Window.m_Width, m_Window.m_Height);

            // pList->SetVertexBuffer(&vertexBuffer);
            // pList->SetIndexBuffer(&indexBuffer);
            // pList->DrawIndexed(3);

            // pList->EndRenderPass();
            // m_pAPI->EndCommandList(pList);
            // m_pAPI->ExecuteCommandList(pList, false);

            // imguiHandler.NewFrame();
            // ImGui::Begin("Hello");
            // ImGui::Button("a");
            // ImGui::ProgressBar(100.5, ImVec2(0, 0), "Test");
            // ImGui::Image(0, ImVec2(512, 128));
            // ImGui::End();
            // imguiHandler.EndFrame();

            m_pAPI->Frame();
            m_Window.Poll();
        }
    }

}  // namespace lr