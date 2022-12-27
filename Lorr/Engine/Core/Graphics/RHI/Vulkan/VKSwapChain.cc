#include "VKSwapChain.hh"

#include "VKAPI.hh"

namespace lr::Graphics
{
    void VKSwapChain::Init(BaseWindow *pWindow, VKAPI *pAPI, SwapChainFlags flags)
    {
        ZoneScoped;

        m_vSync = (flags & SwapChainFlags::vSync);
        (flags & SwapChainFlags::TripleBuffering) ? m_FrameCount = 3 : 2;

        // Calculate window metrics
        m_Width = pWindow->m_Width;
        m_Height = pWindow->m_Height;

        pAPI->GetSurfaceCapabilities(m_SurfaceCapabilities);

        if (!pAPI->IsFormatSupported(m_ImageFormat, &m_ColorSpace))
        {
            m_ImageFormat = LR_RESOURCE_FORMAT_BGRA8F;
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
        imageDesc.Format = m_ImageFormat;

        ImageData imageData;
        imageData.Width = m_Width;
        imageData.Height = m_Height;

        for (u32 i = 0; i < m_FrameCount; i++)
        {
            VKSwapChainFrame &frame = m_pFrames[i];
            VKImage &currentImage = frame.Image;

            // Swapchain already gives us the image, so we don't need to create it again
            currentImage.m_pHandle = ppSwapChainImages[i];

            currentImage.m_UsageFlags = LR_RESOURCE_USAGE_PRESENT;
            currentImage.m_Width = m_Width;
            currentImage.m_Height = m_Height;
            currentImage.m_DataSize = ~0;
            currentImage.m_UsingMip = 0;
            currentImage.m_TotalMips = 1;
            currentImage.m_Format = m_ImageFormat;
            currentImage.m_RequiredDataSize = ~0;
            currentImage.m_AllocatorType = AllocatorType::Count;

            pAPI->CreateImageView(&currentImage);

            /// Create Semaphores

            frame.pAcquireSemp = pAPI->CreateSemaphore();
            frame.pPresentSemp = pAPI->CreateSemaphore();
        }
    }

    BaseImage *VKSwapChain::GetCurrentImage()
    {
        ZoneScoped;

        return &m_pFrames[m_CurrentFrame].Image;
    }

    VKSwapChainFrame *VKSwapChain::GetCurrentFrame()
    {
        ZoneScoped;

        return &m_pFrames[m_CurrentFrame];
    }

    VKSwapChainFrame *VKSwapChain::GetNextFrame()
    {
        ZoneScoped;

        u32 nextFrameIdx = (m_CurrentFrame + 1) % m_FrameCount;
        return &m_pFrames[nextFrameIdx];
    }

    void VKSwapChain::DestroyHandle(VKAPI *pAPI)
    {
        ZoneScoped;

        for (u32 i = 0; i < m_FrameCount; i++)
        {
            VKSwapChainFrame &frame = m_pFrames[i];

            pAPI->DeleteSemaphore(frame.pPresentSemp);
            pAPI->DeleteSemaphore(frame.pAcquireSemp);
        }

        pAPI->DeleteSwapChain(this);
    }

}  // namespace lr::Graphics