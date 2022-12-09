#include "RendererManager.hh"

#include "Core/Graphics/RHI/Vulkan/VKAPI.hh"
#include "Core/Graphics/RHI/D3D12/D3D12API.hh"

namespace lr::Renderer
{
    using namespace Graphics;

    void RendererManager::Init(APIType type, Graphics::APIFlags flags, BaseWindow *pWindow)
    {
        ZoneScoped;

        switch (type)
        {
            case APIType::Vulkan: m_pAPI = new VKAPI; break;
            case APIType::D3D12: m_pAPI = new D3D12API; break;
        }

        m_pAPI->Init(pWindow, pWindow->m_Width, pWindow->m_Height, flags);

        Camera3DDesc camera3DDesc;
        camera3DDesc.Position = XMFLOAT3(0.0, 0.0, 0.0);
        camera3DDesc.ViewSize = XMFLOAT2(pWindow->m_Width, pWindow->m_Height);
        camera3DDesc.ViewDirection = XMFLOAT3(0.0, 0.0, 1.0);
        camera3DDesc.UpDirection = XMFLOAT3(0.0, 1.0, 0.0);
        camera3DDesc.FOV = 60.0;
        camera3DDesc.ZFar = 10000.0;
        camera3DDesc.ZNear = 0.1;

        m_Camera3D.Init(&camera3DDesc);

        Camera2DDesc camera2DDesc;
        camera2DDesc.Position = XMFLOAT2(0.0, 0.0);
        camera2DDesc.ViewSize = XMFLOAT2(pWindow->m_Width, pWindow->m_Height);
        camera2DDesc.ZFar = 1.0;
        camera2DDesc.ZNear = 0.0;

        m_Camera2D.Init(&camera2DDesc);

        InitPasses();
    }

    void RendererManager::Shutdown()
    {
        ZoneScoped;
    }

    void RendererManager::Resize(u32 width, u32 height)
    {
        ZoneScoped;

        m_pAPI->ResizeSwapChain(width, height);
        m_Camera2D.SetSize({ (float)width, (float)height });
    }

    void RendererManager::Poll()
    {
        ZoneScoped;

        m_pAPI->BeginFrame();

        m_Camera2D.CalculateView();
        m_pAPI->CalcOrthoProjection(m_Camera2D.m_Projection, m_Camera2D.m_ViewSize, m_Camera2D.m_ZFar, m_Camera2D.m_ZNear);
        XMMATRIX mat2D = XMMatrixMultiplyTranspose(m_Camera2D.m_View, m_Camera2D.m_Projection);

        BaseImage *pCurrentImage = m_pAPI->GetSwapChain()->GetCurrentImage();

        BaseCommandList *pList = m_pAPI->GetCommandList();
        m_pAPI->BeginCommandList(pList);

        pList->BarrierTransition(pCurrentImage, ResourceUsage::Undefined, ShaderStage::None, ResourceUsage::RenderTarget, ShaderStage::None);

        m_pAPI->EndCommandList(pList);
        m_pAPI->ExecuteCommandList(pList, false);

        m_ImGuiPass.Draw(m_pAPI, mat2D);

        pList = m_pAPI->GetCommandList();
        m_pAPI->BeginCommandList(pList);

        pList->BarrierTransition(pCurrentImage, ResourceUsage::RenderTarget, ShaderStage::None, ResourceUsage::Present, ShaderStage::None);

        m_pAPI->EndCommandList(pList);
        m_pAPI->ExecuteCommandList(pList, false);

        m_pAPI->EndFrame();
    }

    void RendererManager::InitPasses()
    {
        ZoneScoped;

        m_ImGuiPass.Init(m_pAPI);
    }

}  // namespace lr::Renderer