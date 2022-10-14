#include "VKSwapChain.hh"

#include "VKAPI.hh"

namespace lr::Graphics
{
    void VKSwapChain::CreateHandle(VKAPI *pAPI)
    {
        ZoneScoped;

        VkSwapchainCreateInfoKHR swapChainInfo = {};
        swapChainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;

        // Buffer description
        swapChainInfo.minImageCount = m_FrameCount;
        swapChainInfo.imageFormat = m_ImageFormat;
        swapChainInfo.imageColorSpace = m_ColorSpace;
        swapChainInfo.imageExtent.width = m_Width;
        swapChainInfo.imageExtent.height = m_Height;
        swapChainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;

        swapChainInfo.queueFamilyIndexCount = 0;
        swapChainInfo.pQueueFamilyIndices = nullptr;

        swapChainInfo.imageArrayLayers = 1;
        swapChainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

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

        // Create renderpass
        RenderPassAttachment colorAttachment;
        colorAttachment.Format = ResourceFormat::RGBA8F;
        colorAttachment.LoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.StoreOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.FinalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        RenderPassAttachment depthAttachment;
        depthAttachment.Format = ResourceFormat::D32FS8U;
        depthAttachment.LoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.StoreOp = VK_ATTACHMENT_STORE_OP_STORE;
        depthAttachment.StencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.StencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
        depthAttachment.FinalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        RenderPassSubpassDesc subpassDesc;
        subpassDesc.ColorAttachmentCount = 1;
        subpassDesc.ppColorAttachments[0] = &colorAttachment;
        subpassDesc.pDepthAttachment = &depthAttachment;

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

        ImageDesc depthDesc;
        depthDesc.Format = ResourceFormat::D32FS8U;
        depthDesc.Type = ResourceType::DepthAttachment;

        ImageData depthData;
        depthData.Width = m_Width;
        depthData.Height = m_Height;

        for (u32 i = 0; i < m_FrameCount; i++)
        {
            SwapChainFrame &frame = m_pFrames[i];

            /// Create Image Data
            VKImage &currentImage = frame.Image;
            VKImage &currentDepthImage = frame.DepthImage;

            // Swapchain already gives us the image, so we don't need to create it again
            currentImage.m_pHandle = ppSwapChainImages[i];
            currentImage.m_Type = imageDesc.Type;
            currentImage.m_Format = imageDesc.Format;
            currentImage.m_TotalMips = 1;

            pAPI->CreateImageView(&currentImage);

            pAPI->CreateImage(&currentDepthImage, &depthDesc, &depthData);
            pAPI->AllocateImageMemory(&currentDepthImage, AllocatorType::ImageTLSF);
            pAPI->BindMemory(&currentDepthImage);

            pAPI->CreateImageView(&currentDepthImage);

            VKImage pAttachments[2] = { currentImage, currentDepthImage };
            frame.pFrameBuffer = pAPI->CreateFramebuffer(XMUINT2(m_Width, m_Height), 2, pAttachments, m_pRenderPass);

            /// Create Semaphores

            frame.pAcquireSemp = pAPI->CreateSemaphore();
            frame.pPresentSemp = pAPI->CreateSemaphore();
            frame.pFence = pAPI->CreateFence(true);
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

    void VKSwapChain::Init(PlatformWindow *pWindow, VKAPI *pAPI, SwapChainFlags flags)
    {
        ZoneScoped;

        m_vSync = (flags & SwapChainFlags::vSync);
        m_Tearing = (flags & SwapChainFlags::AllowTearing);
        (flags & SwapChainFlags::TripleBuffering) ? m_FrameCount = 3 : 2;

        // Calculate window metrics
        m_Width = pWindow->m_Width;
        m_Height = pWindow->m_Height;

        pAPI->GetSurfaceCapabilities(m_SurfaceCapabilities);

        if (!pAPI->IsFormatSupported(m_ImageFormat, &m_ColorSpace))
        {
            m_ImageFormat = VK_FORMAT_B8G8R8A8_UNORM;
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

    void VKSwapChain::NextFrame()
    {
        m_CurrentFrame = (m_CurrentFrame + 1) % m_FrameCount;
    }

    XMUINT2 VKSwapChain::GetViewportSize()
    {
        return XMUINT2(m_Width, m_Height);
    }

    void VKSwapChain::DestroyHandle(VKAPI *pAPI)
    {
        ZoneScoped;

        for (u32 i = 0; i < m_FrameCount; i++)
        {
            SwapChainFrame &frame = m_pFrames[i];

            pAPI->DeleteFence(frame.pFence);
            pAPI->DeleteSemaphore(frame.pPresentSemp);
            pAPI->DeleteSemaphore(frame.pAcquireSemp);
            pAPI->DeleteFramebuffer(frame.pFrameBuffer);
            pAPI->DeleteImage(&frame.DepthImage);
        }

        pAPI->DeleteRenderPass(m_pRenderPass);
        pAPI->DeleteSwapChain(this);
    }

}  // namespace lr::Graphics