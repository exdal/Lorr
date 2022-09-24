#include "VKSwapChain.hh"

#include "VKAPI.hh"

namespace lr::g
{
    void VKSwapChain::CreateBackBuffer(VKAPI *pAPI)
    {
        ZoneScoped;

        VkDevice &pDevice = pAPI->m_pDevice;

        // Create renderpass
        RenderPassAttachment colorAttachment;
        colorAttachment.Format = ResourceFormat::RGBA8F;
        colorAttachment.LoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.StoreOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.FinalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        RenderPassSubpassDesc subpassDesc;
        subpassDesc.ColorAttachmentCount = 1;
        subpassDesc.ppColorAttachments[0] = &colorAttachment;

        RenderPassDesc renderPassDesc;
        renderPassDesc.pSubpassDesc = &subpassDesc;
        m_pRenderPass = pAPI->CreateRenderPass(&renderPassDesc);

        // Get images
        VkImage ppSwapChainImages[3];
        pAPI->GetSwapChainImages(m_pHandle, m_FrameCount, ppSwapChainImages);

        ImageDesc imageDesc;
        imageDesc.Format = m_Format;

        ImageData imageData;
        imageData.Width = m_Width;
        imageData.Height = m_Height;

        for (u32 i = 0; i < m_FrameCount; i++)
        {
            SwapChainFrame &frame = m_pFrames[i];

            /// Create Image Data
            VKImage &currentImage = frame.Image;
            currentImage.Delete();

            currentImage.Init(&imageDesc, &imageData);

            // Swapchain already gives us the image, so we don't need to create it again
            currentImage.m_pHandle = ppSwapChainImages[i];
            pAPI->CreateImageView(&currentImage, ResourceType::ShaderResource, m_Format, 1);
            frame.pFrameBuffer = pAPI->CreateFramebuffer(XMUINT2(m_Width, m_Height), 1, &currentImage, m_pRenderPass);

            /// Create Semaphores

            frame.pAcquireSemp = pAPI->CreateSemaphore();
            frame.pPresentSemp = pAPI->CreateSemaphore();
            frame.pFence = pAPI->CreateFence(i != 0);
        }
    }

    SwapChainFrame *VKSwapChain::GetCurrentFrame()
    {
        ZoneScoped;

        return &m_pFrames[m_CurrentFrame];
    }

    SwapChainFrame *VKSwapChain::GetNextFrame()
    {
        ZoneScoped;

        u32 nextFrameIdx = (m_CurrentFrame + 1) % m_FrameCount;
        return &m_pFrames[nextFrameIdx];
    }

    void VKSwapChain::Init(PlatformWindow *pWindow, VKAPI *pAPI, u32 queueIndex, SwapChainFlags flags)
    {
        ZoneScoped;

        m_vSync = (flags & SwapChainFlags::vSync);
        m_Tearing = (flags & SwapChainFlags::AllowTearing);
        (flags & SwapChainFlags::TripleBuffering) ? m_FrameCount = 3 : 2;

        /// Calculate window metrics
        m_Width = pWindow->GetWidth();
        m_Height = pWindow->GetHeight();

        VkSurfaceCapabilitiesKHR capabilities;
        pAPI->GetSurfaceCapabilities(capabilities);

        VkFormat imageFormat = VK_FORMAT_B8G8R8A8_UNORM;
        VkColorSpaceKHR colorSpace;
        if (!pAPI->IsFormatSupported(imageFormat, &colorSpace))
        {
            imageFormat = VK_FORMAT_B8G8R8A8_UNORM;
            pAPI->IsFormatSupported(VK_FORMAT_B8G8R8A8_UNORM, &colorSpace);
        }

        VkPresentModeKHR presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
        if (!pAPI->IsPresentModeSupported(presentMode))
        {
            presentMode = VK_PRESENT_MODE_FIFO_KHR;
            m_FrameCount = capabilities.minImageCount;

            LOG_WARN("GPU doesn't support mailbox present mode, going back to fifo.");
        }

        VkSwapchainCreateInfoKHR swapChainInfo = {};
        swapChainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;

        // Buffer description
        swapChainInfo.minImageCount = m_FrameCount;
        swapChainInfo.imageFormat = imageFormat;
        swapChainInfo.imageColorSpace = colorSpace;
        swapChainInfo.imageExtent.width = m_Width;
        swapChainInfo.imageExtent.height = m_Height;
        swapChainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;

        swapChainInfo.queueFamilyIndexCount = 0;
        swapChainInfo.pQueueFamilyIndices = nullptr;

        swapChainInfo.imageArrayLayers = 1;
        swapChainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        swapChainInfo.presentMode = presentMode;
        swapChainInfo.preTransform = capabilities.currentTransform;
        swapChainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        swapChainInfo.surface = pAPI->m_pSurface;
        swapChainInfo.clipped = VK_TRUE;

        pAPI->CreateSwapChain(m_pHandle, swapChainInfo);

        /// Check if we are in fullscreen
    }

    void VKSwapChain::Resize(u32 width, u32 height)
    {
        ZoneScoped;
    }

    void VKSwapChain::NextFrame()
    {
        m_CurrentFrame = (m_CurrentFrame + 1) % m_FrameCount;
    }

    XMUINT2 VKSwapChain::GetViewportSize()
    {
        return XMUINT2(0, 0);
    }

}  // namespace lr::g