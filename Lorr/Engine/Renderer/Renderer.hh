#pragma once

#include "Graphics/Device.hh"
#include "Graphics/Instance.hh"
#include "Graphics/PhysicalDevice.hh"

#include "TaskGraph.hh"

namespace lr
{
struct BaseWindow;
}

namespace lr::Renderer
{
struct FrameManagerDesc
{
    u32 m_FrameCount = 0;
    Graphics::CommandTypeMask m_Types = {};
    Graphics::PhysicalDevice *m_pPhysicalDevice = nullptr;
    Graphics::Device *m_pDevice = nullptr;
    Graphics::SwapChain *m_pSwapChain = nullptr;
};

struct FrameManager
{
    void Init(const FrameManagerDesc &desc);
    void NextFrame();  // Advance to the next frame
    u32 CurrentFrame() { return m_CurrentFrame; }
    eastl::pair<Graphics::Image *, Graphics::ImageView *> GetImages(u32 idx = 0);
    eastl::pair<Graphics::Semaphore *, Graphics::Semaphore *> GetSemaphores(u32 idx = 0);

    u32 m_FrameCount = 0;
    u32 m_CurrentFrame = 0;
    Graphics::CommandTypeMask m_CommandTypes = {};
    Graphics::Device *m_pDevice = nullptr;

    // Prsentation Engine
    eastl::vector<Graphics::Semaphore *> m_AcquireSemas = {};
    eastl::vector<Graphics::Semaphore *> m_PresentSemas = {};
    eastl::vector<Graphics::Image *> m_Images = {};
    eastl::vector<Graphics::ImageView *> m_Views = {};
};

struct Renderer
{
    void Init(BaseWindow *pWindow);
    void Draw();

    Graphics::Instance m_Instance = {};
    Graphics::PhysicalDevice *m_pPhysicalDevice = nullptr;
    Graphics::Surface *m_pSurface = nullptr;
    Graphics::Device *m_pDevice = nullptr;
    Graphics::SwapChain *m_pSwapChain = nullptr;

    FrameManager m_FrameMan = {};
    TaskGraph m_TaskGraph = {};

    ImageID m_SwapChainImage = ImageNull;
};
}  // namespace lr::Renderer