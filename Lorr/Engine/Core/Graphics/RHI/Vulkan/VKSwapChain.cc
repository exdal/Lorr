#include "VKSwapChain.hh"

#include "VKAPI.hh"

namespace lr::Graphics
{
    void VKSwapChain::Init(BaseWindow *pWindow, VKAPI *pAPI, SwapChainFlags flags)
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

    void VKSwapChain::CreateHandle(VKAPI *pAPI)
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

    void VKSwapChain::CreateBackBuffers(VKAPI *pAPI)
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
            VKSwapChainFrame &frame = m_pFrames[i];
            VKImage &currentImage = frame.m_Image;

            // Swapchain already gives us the image, so we don't need to create it again
            currentImage.m_pHandle = ppSwapChainImages[i];

            currentImage.m_UsageFlags = LR_RESOURCE_USAGE_PRESENT;
            currentImage.m_Width = m_Width;
            currentImage.m_Height = m_Height;
            currentImage.m_DataLen = ~0;
            currentImage.m_MipMapLevels = 1;
            currentImage.m_Format = m_ImageFormat;
            currentImage.m_DeviceDataLen = ~0;
            currentImage.m_TargetAllocator = LR_API_ALLOCATOR_COUNT;

            pAPI->CreateImageView(&currentImage);

            frame.m_pAcquireSemp = pAPI->CreateSemaphore();
            frame.m_pPresentSemp = pAPI->CreateSemaphore();
        }
    }

    Image *VKSwapChain::GetCurrentImage()
    {
        ZoneScoped;
        
        return &m_pFrames[m_CurrentFrame].m_Image;
    }

    VKSwapChainFrame *VKSwapChain::GetCurrentFrame()
    {
        ZoneScoped;

        return &m_pFrames[m_CurrentFrame];
    }

    VKSwapChainFrame *VKSwapChain::GetNextFrame()
    {
        ZoneScoped;

        return &m_pFrames[(m_CurrentFrame + 1) % m_FrameCount];
    }

    void VKSwapChain::DestroyHandle(VKAPI *pAPI)
    {
        ZoneScoped;

        for (u32 i = 0; i < m_FrameCount; i++)
        {
            VKSwapChainFrame &frame = m_pFrames[i];

            pAPI->DeleteSemaphore(frame.m_pPresentSemp);
            pAPI->DeleteSemaphore(frame.m_pAcquireSemp);
        }

        pAPI->DeleteSwapChain(this);
    }

}  // namespace lr::Graphics