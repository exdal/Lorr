// Created on Monday July 18th 2022 by exdal
// Last modified on Monday August 28th 2023 by exdal

#pragma once

#include <EASTL/queue.h>

#include "APIAllocator.hh"
#include "APIResource.hh"
#include "Common.hh"

#include "CommandList.hh"

namespace lr::Graphics
{
struct SwapChainFrame
{
    eastl::queue<Buffer *> m_BufferDeleteQueue;
    eastl::queue<Image *> m_ImageDeleteQueue;

    Image *m_pImage = nullptr;

    Semaphore *m_pAcquireSemp = nullptr;
    Semaphore *m_pPresentSemp = nullptr;
};

struct SwapChain : APIObject<VK_OBJECT_TYPE_SWAPCHAIN_KHR>
{
    void Advance(u32 nextImage, Semaphore *pAcquireSemp);
    SwapChainFrame *GetCurrentFrame();

    VkSwapchainKHR m_pHandle = nullptr;

    /// INTERNALS ///
    u32 m_CurrentFrame = 0;
    u32 m_FrameCount = 2;
    u32 m_Width = 0;
    u32 m_Height = 0;
    bool m_vSync = false;
    eastl::array<SwapChainFrame, Limits::MaxFrameCount> m_Frames = {};

    Format m_ImageFormat = Format::BGRA8_UNORM;
    VkColorSpaceKHR m_ColorSpace = VK_COLOR_SPACE_MAX_ENUM_KHR;
    VkPresentModeKHR m_PresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
};

}  // namespace lr::Graphics