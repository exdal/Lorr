#include "TriangleApp.hh"

#include "Graphics/Device.hh"
#include "Graphics/Instance.hh"
#include "Graphics/PhysicalDevice.hh"

using namespace lr;

struct RendererCtx
{
    Graphics::Instance m_Instance = {};
    Graphics::PhysicalDevice *m_pPhysicalDevice = nullptr;
    Graphics::Surface *m_pSurface = nullptr;
    Graphics::Device *m_pDevice = nullptr;
    Graphics::SwapChain *m_pSwapChain = nullptr;
    Graphics::CommandQueue *m_pQueue = nullptr;

    struct Frame
    {
        Graphics::CommandAllocator *m_pAllocator = nullptr;
        Graphics::CommandList *m_pList = nullptr;
        Graphics::Semaphore *m_pAcquireSema = nullptr;
        Graphics::Semaphore *m_pPresentSema = nullptr;
        Graphics::Semaphore *m_pSyncSema = nullptr;
        Graphics::Image *m_pImage = nullptr;
    };
    eastl::vector<Frame> m_Frames = {};

} renderer;

void TriangleApp::Init(lr::BaseApplicationDesc &desc)
{
    ZoneScoped;

    PreInit(desc);

    auto pWindow = Engine::GetWindow();

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

    if (!renderer.m_Instance.Init(&instanceDesc))
        return;

    renderer.m_pPhysicalDevice = renderer.m_Instance.GetPhysicalDevice();
    if (!renderer.m_pPhysicalDevice)
        return;

    renderer.m_pSurface = renderer.m_Instance.GetWin32Surface((Win32Window *)pWindow);
    if (!renderer.m_pSurface)
        return;

    renderer.m_pPhysicalDevice->SetSurfaceCapabilities(renderer.m_pSurface);
    renderer.m_pPhysicalDevice->GetSurfaceFormats(
        renderer.m_pSurface, renderer.m_pSurface->m_SurfaceFormats);
    renderer.m_pPhysicalDevice->GetPresentModes(
        renderer.m_pSurface, renderer.m_pSurface->m_PresentModes);
    renderer.m_pPhysicalDevice->InitQueueFamilies(renderer.m_pSurface);
    renderer.m_pDevice = renderer.m_pPhysicalDevice->GetLogicalDevice();
    if (!renderer.m_pDevice)
        return;

    renderer.m_pSwapChain =
        renderer.m_pDevice->CreateSwapChain(renderer.m_pSurface, &swapChainDesc);
    if (!renderer.m_pSwapChain)
        return;

    u32 graphicsIdx =
        renderer.m_pPhysicalDevice->GetQueueIndex(Graphics::CommandType::Graphics);
    renderer.m_pQueue = renderer.m_pDevice->CreateCommandQueue(
        Graphics::CommandType::Graphics, graphicsIdx);

    auto swapChainImages = renderer.m_pDevice->GetSwapChainImages(renderer.m_pSwapChain);
    renderer.m_Frames.resize(swapChainDesc.m_FrameCount);
    for (u32 i = 0; i < swapChainDesc.m_FrameCount; i++)
    {
        RendererCtx::Frame &frame = renderer.m_Frames[i];

        frame.m_pAllocator =
            renderer.m_pDevice->CreateCommandAllocator(renderer.m_pQueue, true);
        frame.m_pList = renderer.m_pDevice->CreateCommandList(frame.m_pAllocator);
        frame.m_pAcquireSema = renderer.m_pDevice->CreateBinarySemaphore();
        frame.m_pPresentSema = renderer.m_pDevice->CreateBinarySemaphore();
        frame.m_pSyncSema = renderer.m_pDevice->CreateTimelineSemaphore(0);
        frame.m_pImage = swapChainImages[i];
    }
}

void TriangleApp::Shutdown()
{
    ZoneScoped;
}

void TriangleApp::Poll(f32 deltaTime)
{
    ZoneScoped;

    static u32 curFrameIdx = 0;
    RendererCtx::Frame &curFrame = renderer.m_Frames[curFrameIdx];
    auto pList = curFrame.m_pList;
    curFrameIdx =
        renderer.m_pDevice->AcquireImage(renderer.m_pSwapChain, curFrame.m_pAcquireSema);

    renderer.m_pDevice->WaitForSemaphore(
        curFrame.m_pSyncSema, curFrame.m_pSyncSema->m_Value);
    renderer.m_pDevice->ResetCommandAllocator(curFrame.m_pAllocator);
    renderer.m_pDevice->BeginCommandList(pList);

    {
        Graphics::PipelineBarrier pipelineInfo = {
            .m_SrcLayout = Graphics::ImageLayout::Undefined,
            .m_DstLayout = Graphics::ImageLayout::ColorReadOnly,
            .m_SrcStage = Graphics::PipelineStage::TopOfPipe,
            .m_DstStage = Graphics::PipelineStage::VertexShader,
            .m_SrcAccess = Graphics::MemoryAccess::None,
            .m_DstAccess = Graphics::MemoryAccess::ShaderRead,
        };
        Graphics::ImageBarrier barrier(curFrame.m_pImage, pipelineInfo);
        Graphics::DependencyInfo depInfo(barrier);
        pList->SetPipelineBarrier(&depInfo);
    }

    Graphics::RenderingAttachment attachment(
        curFrame.m_pImage,
        Graphics::ImageLayout::ColorAttachment,
        Graphics::AttachmentOp::Clear,
        Graphics::AttachmentOp::Store,
        Graphics::ColorClearValue(1.0f, 0.0f, 0.0f, 1.0f));
    Graphics::RenderingBeginDesc beginDesc = {
        .m_RenderArea = { 0, 0, curFrame.m_pImage->m_Width, curFrame.m_pImage->m_Height },
        .m_ColorAttachments = attachment,
    };
    pList->BeginRendering(&beginDesc);
    pList->EndRendering();

    {
        Graphics::PipelineBarrier pipelineInfo = {
            .m_SrcLayout = Graphics::ImageLayout::ColorReadOnly,
            .m_DstLayout = Graphics::ImageLayout::Present,
            .m_SrcStage = Graphics::PipelineStage::PixelShader,
            .m_DstStage = Graphics::PipelineStage::BottomOfPipe,
            .m_SrcAccess = Graphics::MemoryAccess::ShaderRead,
            .m_DstAccess = Graphics::MemoryAccess::None,
        };
        Graphics::ImageBarrier barrier(curFrame.m_pImage, pipelineInfo);
        Graphics::DependencyInfo depInfo(barrier);
        pList->SetPipelineBarrier(&depInfo);
    }

    renderer.m_pDevice->EndCommandList(pList);

    Graphics::SemaphoreSubmitDesc waitSema(
        curFrame.m_pAcquireSema, Graphics::PipelineStage::TopOfPipe);
    Graphics::CommandListSubmitDesc listsDesc(pList);
    Graphics::SemaphoreSubmitDesc pSignalSemas[] = {
        Graphics::SemaphoreSubmitDesc(
            curFrame.m_pPresentSema, Graphics::PipelineStage::AllCommands),
        Graphics::SemaphoreSubmitDesc(
            curFrame.m_pSyncSema,
            ++curFrame.m_pSyncSema->m_Value,
            Graphics::PipelineStage::AllCommands),
    };
    Graphics::SubmitDesc submitDesc = {
        .m_WaitSemas = waitSema,
        .m_Lists = listsDesc,
        .m_SignalSemas = pSignalSemas,
    };
    renderer.m_pDevice->Submit(renderer.m_pQueue, &submitDesc);

    renderer.m_pDevice->Present(
        renderer.m_pSwapChain, curFrameIdx, curFrame.m_pPresentSema, renderer.m_pQueue);

    curFrameIdx = (curFrameIdx + 1) % renderer.m_Frames.size();
}