#include "Renderer.hh"

#include "Window/Win32/Win32Window.hh"

#include "TaskGraph.hh"

namespace lr::Renderer
{
struct ClearSwapChain : Task
{
    constexpr static eastl::string_view kName = "ClearSwapChain";

    struct Uses
    {
        Preset::SwapChainImage m_RenderTarget;
    } m_Uses;

    void Execute(TaskContext &ctx)
    {
        Graphics::CommandList *pList = ctx.GetCommandList();

        // Graphics::RenderingBeginDesc renderingDesc = {
        //    .m_RenderArea = ctx.GetSwapChainArea(),
        //    .m_ColorAttachments = ctx.GetColorAttachments(),
        // };

        // pList->BeginRendering(&renderingDesc);
        // pList->EndRendering();
    }
};

void Renderer::Init(BaseWindow *pWindow)
{
    ZoneScoped;

    Graphics::InstanceDesc instanceDesc = {
        .m_AppName = "Lorr",
        .m_AppVersion = 1,
        .m_EngineName = "Lorr",
        .m_APIVersion = VK_API_VERSION_1_3,
    };

    // TODO: Properly handle swapchain frames
    Graphics::SwapChainDesc swapChainDesc = {
        .m_Width = pWindow->m_Width,
        .m_Height = pWindow->m_Height,
        .m_FrameCount = 3,
        .m_Format = Graphics::Format::BGRA8_UNORM,
        .m_ColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
        .m_PresentMode = VK_PRESENT_MODE_MAILBOX_KHR,
    };

    if (!m_Instance.Init(&instanceDesc))
        return;

    m_pPhysicalDevice = m_Instance.GetPhysicalDevice();
    if (!m_pPhysicalDevice)
        return;

    m_pSurface = m_Instance.GetWin32Surface((Win32Window *)pWindow);
    if (!m_pSurface)
        return;

    m_pPhysicalDevice->SetSurfaceCapabilities(m_pSurface);
    m_pPhysicalDevice->GetSurfaceFormats(m_pSurface, m_pSurface->m_SurfaceFormats);
    m_pPhysicalDevice->GetPresentModes(m_pSurface, m_pSurface->m_PresentModes);
    m_pPhysicalDevice->InitQueueFamilies(m_pSurface);
    m_pDevice = m_pPhysicalDevice->GetLogicalDevice();
    if (!m_pDevice)
        return;

    m_pSwapChain = m_pDevice->CreateSwapChain(m_pSurface, &swapChainDesc);
    if (!m_pSwapChain)
        return;

    auto [ppImages, ppImageViews] = m_pDevice->GetSwapChainImages(m_pSwapChain);
    m_Frames.resize(m_pSwapChain->m_FrameCount);

    TaskGraph graph;
    TaskGraphDesc tgDesc = {};
    graph.Init(&tgDesc);

    graph.AddTask<ClearSwapChain>({}, nullptr);
    graph.CompileGraph();
}

}  // namespace lr::Renderer