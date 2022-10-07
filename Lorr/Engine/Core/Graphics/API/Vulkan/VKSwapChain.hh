//
// Created on Monday 9th May 2022 by e-erdal
//

#pragma once

#include "Window/PlatformWindow.hh"

#include "VKResource.hh"

namespace lr::g
{
    struct VKAPI;

    enum class SwapChainFlags : u8
    {
        None,
        vSync = 1 << 0,
        AllowTearing = 1 << 1,
        TripleBuffering = 1 << 2,
    };
    EnumFlags(SwapChainFlags);

    struct SwapChainFrame
    {
        VKImage Image;
        VKImage DepthImage;

        VkFramebuffer pFrameBuffer = nullptr;

        VkSemaphore pAcquireSemp = nullptr;
        VkSemaphore pPresentSemp = nullptr;
        VkFence pFence = nullptr;
    };

    struct VKSwapChain
    {
        void Init(PlatformWindow *pWindow, VKAPI *pAPI, u32 queueIndex, SwapChainFlags flags);

        void Resize(u32 width, u32 height);
        void NextFrame();

        void CreateBackBuffer(VKAPI *pAPI);
        SwapChainFrame *GetCurrentFrame();
        SwapChainFrame *GetNextFrame();
        XMUINT2 GetViewportSize();

        VkSwapchainKHR m_pHandle = nullptr;
        VkRenderPass m_pRenderPass = nullptr;

        ResourceFormat m_Format = ResourceFormat::RGBA8F;

        u32 m_CurrentFrame = 0;
        u32 m_FrameCount = 2;
        eastl::array<SwapChainFrame, 3> m_pFrames;

        u32 m_Width = 0;
        u32 m_Height = 0;

        bool m_vSync = false;
        bool m_Tearing = false;
    };

}  // namespace lr::g