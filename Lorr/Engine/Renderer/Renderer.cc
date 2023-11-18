#include "Renderer.hh"

#include "Window/Win32/Win32Window.hh"

#include "TaskGraph.hh"

namespace lr::Renderer
{
struct ClearSwapChain
{
    constexpr static eastl::string_view kName = "ClearSwapChain";

    struct Uses
    {
        Preset::ColorAttachment m_RenderTarget;
    } m_Uses = {};

    void Execute(TaskContext &tc)
    {
        auto pList = tc.GetCommandList();
        auto pView = tc.View(m_Uses.m_RenderTarget);

        Graphics::RenderingAttachment attachment(
            pView,
            m_Uses.m_RenderTarget.m_ImageLayout,
            Graphics::AttachmentOp::Clear,
            Graphics::AttachmentOp::Store,
            Graphics::ColorClearValue(0.1f, 0.1f, 0.1f, 1.0f));
        Graphics::RenderingBeginDesc renderingDesc = {
            .m_RenderArea = { 0, 0, 1280, 780 },
            .m_ColorAttachments = attachment,
        };

        pList->BeginRendering(&renderingDesc);
        // Le epic pbr rendering
        pList->EndRendering();
    }
};

void FrameManager::Init(const FrameManagerDesc &desc)
{
    ZoneScoped;

    m_FrameCount = desc.m_FrameCount;
    m_CommandTypes = desc.m_Types;
    m_pDevice = desc.m_pDevice;

    auto scImages = m_pDevice->GetSwapChainImages(desc.m_pSwapChain);
    for (int i = 0; i < desc.m_FrameCount; ++i)
    {
        m_AcquireSemas.push_back(m_pDevice->CreateBinarySemaphore());
        m_PresentSemas.push_back(m_pDevice->CreateBinarySemaphore());
        m_Images.push_back(scImages[i]);
        Graphics::ImageViewDesc viewDesc = { .m_pImage = scImages[i] };
        m_Views.push_back(m_pDevice->CreateImageView(&viewDesc));
    }
}

void FrameManager::NextFrame()
{
    m_CurrentFrame = (m_CurrentFrame + 1) % m_FrameCount;
}

eastl::pair<Graphics::Image *, Graphics::ImageView *> FrameManager::GetImages(u32 idx)
{
    return { m_Images[idx], m_Views[idx] };
}

eastl::pair<Graphics::Semaphore *, Graphics::Semaphore *> FrameManager::GetSemaphores(
    u32 idx)
{
    return { m_AcquireSemas[idx], m_PresentSemas[idx] };
}

void Renderer::Init(BaseWindow *pWindow)
{
    ZoneScoped;

    Graphics::InstanceDesc instanceDesc = {
        .m_AppName = "Lorr",
        .m_AppVersion = 1,
        .m_EngineName = "Lorr",
        .m_APIVersion = VK_API_VERSION_1_3,
    };

    if (!m_Instance.Init(&instanceDesc))
        return;

    m_pPhysicalDevice = m_Instance.GetPhysicalDevice();
    if (!m_pPhysicalDevice)
        return;

    m_pSurface = m_Instance.GetWin32Surface(static_cast<Win32Window *>(pWindow));
    if (!m_pSurface)
        return;

    m_pPhysicalDevice->SetSurfaceCapabilities(m_pSurface);
    m_pPhysicalDevice->GetSurfaceFormats(m_pSurface, m_pSurface->m_SurfaceFormats);
    m_pPhysicalDevice->GetPresentModes(m_pSurface, m_pSurface->m_PresentModes);
    m_pPhysicalDevice->InitQueueFamilies(m_pSurface);
    m_pDevice = m_pPhysicalDevice->GetLogicalDevice();
    if (!m_pDevice)
        return;

    // TODO: Properly handle swapchain frames
    Graphics::SwapChainDesc swapChainDesc = {
        .m_Width = pWindow->m_Width,
        .m_Height = pWindow->m_Height,
        .m_FrameCount = 3,
        .m_Format = Graphics::Format::BGRA8_UNORM,
        .m_ColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
        .m_PresentMode = VK_PRESENT_MODE_MAILBOX_KHR,
    };
    m_pSwapChain = m_pDevice->CreateSwapChain(m_pSurface, &swapChainDesc);

    FrameManagerDesc frameManDesc = {
        .m_FrameCount = m_pSwapChain->m_FrameCount,
        .m_Types = Graphics::CommandTypeMask::Graphics,
        .m_pPhysicalDevice = m_pPhysicalDevice,
        .m_pDevice = m_pDevice,
        .m_pSwapChain = m_pSwapChain,
    };
    m_FrameMan.Init(frameManDesc);

    TaskGraphDesc graphDesc = {
        .m_FrameCount = m_pSwapChain->m_FrameCount,
        .m_pPhysDevice = m_pPhysicalDevice,
        .m_pDevice = m_pDevice,
    };
    m_TaskGraph.Init(&graphDesc);

    auto [swapChainImg, swapChainView] = m_FrameMan.GetImages();
    m_SwapChainImage = m_TaskGraph.UsePersistentImage(
        { .m_pImage = swapChainImg, .m_pImageView = swapChainView });

    m_TaskGraph.AddTask<ClearSwapChain>({
        .m_Uses = { .m_RenderTarget = m_SwapChainImage },
    });
    m_TaskGraph.PresentTask(m_SwapChainImage);
}

void Renderer::Draw()
{
    ZoneScoped;

    auto [acquireSema, presentSema] = m_FrameMan.GetSemaphores(m_FrameMan.CurrentFrame());
    auto [swapChainImg, swapChainView] = m_FrameMan.GetImages(m_FrameMan.CurrentFrame());
    m_pDevice->AcquireImage(m_pSwapChain, acquireSema);

    m_TaskGraph.SetImage(m_SwapChainImage, swapChainImg, swapChainView);
    m_TaskGraph.Execute({
        .m_FrameIndex = m_FrameMan.CurrentFrame(),
        .m_pAcquireSema = acquireSema,
        .m_pPresentSema = presentSema,
    });

    m_pDevice->Present(m_pSwapChain, m_FrameMan.CurrentFrame(), presentSema, m_TaskGraph.m_pGraphicsQueue);
    m_FrameMan.NextFrame();
}

}  // namespace lr::Renderer
