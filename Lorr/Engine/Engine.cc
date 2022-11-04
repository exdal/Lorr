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

    BaseBuffer *pVertexBuffer = nullptr;
    BaseBuffer *pIndexBuffer = nullptr;

    UI::ImGuiRenderer imguiHandler;

    void Engine::Init(ApplicationDesc &desc, WindowDesc &windowDesc)
    {
        ZoneScoped;

        Logger::Init();

        m_Window.Init(windowDesc);

        m_pAPI = new VKAPI;
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

        XMFLOAT3 pData[3] = { { 0, -0.5, 0 }, { 0.5, 0.5, 0 }, { -0.5, 0.5, 0 } };
        u32 pIndexData[3] = { 0, 1, 2 };

        //~ Idea:
        //~ Create a pool that lasts one frame, it would be very useful for staging buffers

        BufferDesc bufDesc = {};
        bufDesc.UsageFlags = ResourceUsage::CopySrc;
        bufDesc.Mappable = true;
        bufDesc.TargetAllocator = AllocatorType::None;

        BufferData bufData = {};
        bufData.DataLen = sizeof(XMFLOAT3) * 3;

        BaseBuffer *pTempVertexBuffer = m_pAPI->CreateBuffer(&bufDesc, &bufData);

        void *pMapData = nullptr;
        m_pAPI->MapMemory(pTempVertexBuffer, pMapData);
        memcpy(pMapData, pData, bufData.DataLen);
        m_pAPI->UnmapMemory(pTempVertexBuffer);

        /// ------------------------------------------------------- ///

        bufDesc.UsageFlags = ResourceUsage::CopySrc;
        bufDesc.Mappable = true;

        bufData.DataLen = sizeof(u32) * 3;

        BaseBuffer *pTempIndexBuffer = m_pAPI->CreateBuffer(&bufDesc, &bufData);

        m_pAPI->MapMemory(pTempIndexBuffer, pMapData);
        memcpy(pMapData, pIndexData, bufData.DataLen);
        m_pAPI->UnmapMemory(pTempIndexBuffer);

        /// ------------------------------------------------------- ///

        BaseCommandList *pList = m_pAPI->GetCommandList();
        m_pAPI->BeginCommandList(pList);

        pVertexBuffer = m_pAPI->ChangeAllocator(pList, pTempIndexBuffer, AllocatorType::BufferTLSF);
        pIndexBuffer = m_pAPI->ChangeAllocator(pList, pTempVertexBuffer, AllocatorType::BufferTLSF);

        pList->BarrierTransition(pVertexBuffer, ResourceUsage::CopyDst, ShaderStage::None, ResourceUsage::VertexBuffer, ShaderStage::None);
        pList->BarrierTransition(pIndexBuffer, ResourceUsage::CopyDst, ShaderStage::None, ResourceUsage::IndexBuffer, ShaderStage::None);

        m_pAPI->EndCommandList(pList);
        m_pAPI->ExecuteCommandList(pList, true);

        m_pAPI->DeleteBuffer(pTempVertexBuffer);
        m_pAPI->DeleteBuffer(pTempIndexBuffer);

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
            f32 deltaTime = timer.elapsed();
            timer.reset();

            m_Camera3D.Update(0, 25.0);
            m_Camera2D.Update(0, 25.0);

            XMMATRIX mat3D = XMMatrixMultiply(m_Camera3D.m_View, m_Camera3D.m_Projection);
            XMMATRIX mat2D = XMMatrixMultiply(m_Camera2D.m_View, m_Camera2D.m_Projection);
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

            BaseCommandList *pList = m_pAPI->GetCommandList();

            m_pAPI->BeginCommandList(pList);

            BaseImage *pImage = m_pAPI->GetSwapChain()->GetCurrentImage();

            pList->BarrierTransition(pImage, ResourceUsage::Undefined, ShaderStage::None, ResourceUsage::RenderTarget, ShaderStage::None);
            pList->BarrierTransition(pImage, ResourceUsage::RenderTarget, ShaderStage::None, ResourceUsage::Present, ShaderStage::None);

            m_pAPI->EndCommandList(pList);
            m_pAPI->ExecuteCommandList(pList, false);

            m_pAPI->Frame();
            m_Window.Poll();
        }
    }

}  // namespace lr