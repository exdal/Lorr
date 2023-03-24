//
// Created on Monday 9th May 2022 by exdal
//

#pragma once

#include "Core/Window/BaseWindow.hh"

#include "Allocator.hh"
#include "VKCommandList.hh"
#include "VKResource.hh"

namespace lr::Graphics
{
enum SwapChainFlags : u8
{
    LR_SWAP_CHAIN_FLAG_NONE,                       // Buffer count = 2
    LR_SWAP_CHAIN_FLAG_VSYNC = 1 << 0,             // Buffer count = 1
    LR_SWAP_CHAIN_FLAG_TRIPLE_BUFFERING = 1 << 2,  // Buffer count = 3
};
EnumFlags(SwapChainFlags);

struct SwapChainFrame
{
    eastl::queue<Buffer *> m_BufferDeleteQueue;
    eastl::queue<Image *> m_ImageDeleteQueue;

    Image *m_pImage = nullptr;

    Semaphore *m_pAcquireSemp = nullptr;
    Semaphore *m_pPresentSemp = nullptr;
};

struct VKSwapChain : APIObject<VK_OBJECT_TYPE_SWAPCHAIN_KHR>
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
    SwapChainFrame m_pFrames[LR_MAX_FRAME_COUNT] = {};

    ImageFormat m_ImageFormat = LR_IMAGE_FORMAT_BGRA8F;
    VkColorSpaceKHR m_ColorSpace = VK_COLOR_SPACE_MAX_ENUM_KHR;
    VkPresentModeKHR m_PresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
};

}  // namespace lr::Graphics