#include "VKSwapChain.hh"

#include "VKAPI.hh"

namespace lr::Graphics
{
    void SwapChain::Init(BaseWindow *pWindow, VKAPI *pAPI, SwapChainFlags flags)
    {
        ZoneScoped;

        m_vSync = (flags & LR_SWAP_CHAIN_FLAG_VSYNC);
        (flags & LR_SWAP_CHAIN_FLAG_TRIPLE_BUFFERING) ? m_FrameCount = 3 : 2;

        // Calculate window metrics
        m_Width = pWindow->m_Width;
        m_Height = pWindow->m_Height;

        pAPI->GetSurfaceCapabilities(m_SurfaceCapabilities);

        if (!pAPI->IsFormatSupported(m_ImageFormat, &m_ColorSpace))
        {
            m_ImageFormat = LR_IMAGE_FORMAT_BGRA8F;
            pAPI->IsFormatSupported(m_ImageFormat, &m_ColorSpace);
        }

        if (!pAPI->IsPresentModeSupported(m_PresentMode))
        {
            m_PresentMode = VK_PRESENT_MODE_FIFO_KHR;
            m_FrameCount = m_SurfaceCapabilities.minImageCount;

            LOG_WARN("GPU doesn't support mailbox present mode, going back to fifo.");
        }

        CreateHandle(pAPI);
    }

    void SwapChain::CreateHandle(VKAPI *pAPI)
    {
        ZoneScoped;

        VkSwapchainCreateInfoKHR swapChainInfo = {};
        swapChainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        swapChainInfo.pNext = nullptr;

        // Buffer description
        swapChainInfo.minImageCount = m_FrameCount;
        swapChainInfo.imageFormat = VKAPI::ToVKFormat(m_ImageFormat);
        swapChainInfo.imageColorSpace = m_ColorSpace;
        swapChainInfo.imageExtent.width = m_Width;
        swapChainInfo.imageExtent.height = m_Height;
        swapChainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;

        swapChainInfo.queueFamilyIndexCount = 0;
        swapChainInfo.pQueueFamilyIndices = nullptr;

        swapChainInfo.imageArrayLayers = 1;
        swapChainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        swapChainInfo.presentMode = m_PresentMode;
        swapChainInfo.preTransform = m_SurfaceCapabilities.currentTransform;
        swapChainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        swapChainInfo.surface = pAPI->m_pSurface;
        swapChainInfo.clipped = VK_TRUE;

        pAPI->CreateSwapChain(m_pHandle, swapChainInfo);

        m_CurrentFrame = 0;
    }

    void SwapChain::DestroyHandle(VKAPI *pAPI)
    {
        ZoneScoped;

        for (u32 i = 0; i < m_FrameCount; i++)
        {
            SwapChainFrame &frame = m_pFrames[i];

            pAPI->DeleteSemaphore(frame.m_pPresentSemp);
            pAPI->DeleteSemaphore(frame.m_pAcquireSemp);
        }

        pAPI->DeleteSwapChain(this);
    }

    void SwapChain::Advance(u32 nextImage, Semaphore *pAcquireSemp)
    {
        ZoneScoped;

        m_CurrentFrame = nextImage;
        m_pFrames[m_CurrentFrame].m_pAcquireSemp = pAcquireSemp;
    }

    void SwapChain::CreateBackBuffers(VKAPI *pAPI)
    {
        ZoneScoped;

        VkDevice &pDevice = pAPI->m_pDevice;

        // Get images
        VkImage ppSwapChainImages[3];
        pAPI->GetSwapChainImages(m_pHandle, m_FrameCount, ppSwapChainImages);

        ImageDesc imageDesc;
        imageDesc.m_Format = m_ImageFormat;
        imageDesc.m_Width = m_Width;
        imageDesc.m_Height = m_Height;

        for (u32 i = 0; i < m_FrameCount; i++)
        {
            SwapChainFrame &frame = m_pFrames[i];
            Image *pCurrentImage = frame.m_pImage = new Image;

            // Swapchain already gives us the image, so we don't need to create it again
            pCurrentImage->m_pHandle = ppSwapChainImages[i];
            pAPI->SetObjectName(pCurrentImage, Format("Swap Chain Image {}", i));

            pCurrentImage->m_UsageFlags = LR_IMAGE_USAGE_RENDER_TARGET;
            pCurrentImage->m_Width = m_Width;
            pCurrentImage->m_Height = m_Height;
            pCurrentImage->m_DataLen = ~0;
            pCurrentImage->m_MipMapLevels = 1;
            pCurrentImage->m_Format = m_ImageFormat;
            pCurrentImage->m_DeviceDataLen = ~0;
            pCurrentImage->m_TargetAllocator = LR_API_ALLOCATOR_COUNT;

            pAPI->CreateImageView(pCurrentImage);

            frame.m_pPresentSemp = pAPI->CreateSemaphore();
            pAPI->SetObjectName(frame.m_pPresentSemp, Format("Present Semaphore {}", i));
        }
    }

}  // namespace lr::Graphics