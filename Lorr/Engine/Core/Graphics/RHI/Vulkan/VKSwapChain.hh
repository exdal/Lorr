//
// Created on Monday 9th May 2022 by e-erdal
//

#pragma once

#include "Core/Window/BaseWindow.hh"

#include "Core/Graphics/RHI/Base/BaseSwapChain.hh"

#include "VKResource.hh"

namespace lr::Graphics
{
    struct VKSwapChainFrame
    {
        VKImage m_Image;

        VkSemaphore m_pAcquireSemp = nullptr;
        VkSemaphore m_pPresentSemp = nullptr;
    };

    struct VKAPI;
    struct VKSwapChain : public BaseSwapChain
    {
        void Init(BaseWindow *pWindow, VKAPI *pAPI, SwapChainFlags flags);

        void CreateHandle(VKAPI *pAPI);
        void CreateBackBuffers(VKAPI *pAPI);
        Image *GetCurrentImage();
        VKSwapChainFrame *GetCurrentFrame();
        VKSwapChainFrame *GetNextFrame();

        void DestroyHandle(VKAPI *pAPI);

        VkSwapchainKHR m_pHandle = nullptr;

        eastl::array<VKSwapChainFrame, 3> m_pFrames;

        /// INTERNALS ///
        VkSurfaceCapabilitiesKHR m_SurfaceCapabilities;
        VkColorSpaceKHR m_ColorSpace;
        VkPresentModeKHR m_PresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
    };

}  // namespace lr::Graphics