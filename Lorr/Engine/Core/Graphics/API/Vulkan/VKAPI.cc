#include "VKAPI.hh"

#include "Crypto/Light.hh"

/// DEFINE VULKAN FUNCTIONS
// #include "VKSymbols.hh"
#define _VK_DEFINE_FUNCTION(_name) PFN_##_name _name
// Define symbols
_VK_IMPORT_SYMBOLS
#undef _VK_DEFINE_FUNCTION

#include "VKCommandList.hh"

#define VKCall(fn, err)                            \
    if (fn != VK_SUCCESS)                          \
    {                                              \
        LOG_CRITICAL(err " - Code: {}", vkResult); \
    }

#define VKCallRet(fn, err, ret)                    \
    if (fn != VK_SUCCESS)                          \
    {                                              \
        LOG_CRITICAL(err " - Code: {}", vkResult); \
        return ret;                                \
    }

namespace lr::g
{
    bool VKAPI::Init(PlatformWindow *pWindow, u32 width, u32 height, APIFlags flags)
    {
        ZoneScoped;

        LOG_TRACE("Initializing Vulkan device...");

        LoadVulkan();
        SetupInstance(pWindow->GetHandle());

        u32 availableDeviceCount = 0;
        vkEnumeratePhysicalDevices(m_pInstance, &availableDeviceCount, nullptr);

        if (availableDeviceCount == 0)
        {
            LOG_CRITICAL("No GPU with Vulkan support.");
            return false;
        }

        VkPhysicalDevice *ppAvailableDevices = new VkPhysicalDevice[availableDeviceCount];
        vkEnumeratePhysicalDevices(m_pInstance, &availableDeviceCount, ppAvailableDevices);

        /// Select device

        VkPhysicalDeviceFeatures deviceFeatures;

        for (u32 i = 0; i < availableDeviceCount; i++)
        {
            VkPhysicalDevice pCurDevice = ppAvailableDevices[i];

            VkPhysicalDeviceProperties deviceProps;

            vkGetPhysicalDeviceProperties(pCurDevice, &deviceProps);
            vkGetPhysicalDeviceFeatures(pCurDevice, &deviceFeatures);

            if (deviceProps.deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
            {
                // We want to run on real GPU
                continue;
            }

            if (!deviceFeatures.tessellationShader || !deviceFeatures.geometryShader || !deviceFeatures.fullDrawIndexUint32)
            {
                continue;
            }

            m_pPhysicalDevice = pCurDevice;

            break;
        }

        if (!SetupQueues(m_pPhysicalDevice, deviceFeatures))
            return false;

        if (!SetupDevice(m_pPhysicalDevice, deviceFeatures))
            return false;

        /// Get surface formats
        vkGetPhysicalDeviceSurfaceFormatsKHR(m_pPhysicalDevice, m_pSurface, &m_ValidSurfaceFormatCount, nullptr);

        m_pValidSurfaceFormats = new VkSurfaceFormatKHR[m_ValidSurfaceFormatCount];
        vkGetPhysicalDeviceSurfaceFormatsKHR(m_pPhysicalDevice, m_pSurface, &m_ValidSurfaceFormatCount, m_pValidSurfaceFormats);

        /// Get surface present modes
        vkGetPhysicalDeviceSurfacePresentModesKHR(m_pPhysicalDevice, m_pSurface, &m_ValidSurfaceFormatCount, nullptr);

        m_pValidPresentModes = new VkPresentModeKHR[m_ValidPresentModeCount];
        vkGetPhysicalDeviceSurfacePresentModesKHR(m_pPhysicalDevice, m_pSurface, &m_ValidSurfaceFormatCount, m_pValidPresentModes);

        // yeah uh, dont delete
        // delete[] ppAvailableDevices;

        CreateCommandQueue(&m_DirectQueue, CommandListType::Direct);
        m_SwapChain.Init(pWindow, this, 0, SwapChainFlags::vSync | SwapChainFlags::TripleBuffering);
        m_SwapChain.CreateBackBuffer(this);
        m_PipelineManager.Init(this);

        // LOG_TRACE("Successfully initialized D3D12 device.");

        /// INITIALIZE RENDER CONTEXT ///
        m_CommandListPool.Init();
        m_pDescriptorPool = CreateDescriptorPool({
            { 0, DescriptorType::ConstantBuffer, VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM, 256 },
            { 0, DescriptorType::Texture, VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM, 256 },
            { 0, DescriptorType::Sampler, VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM, 256 },
        });

        /// In the future, we will have worker threads, i believe in you...
        for (u32 i = 0; i < m_CommandListPool.m_DirectLists.count; i++)
        {
            VKCommandList &list = m_CommandListPool.m_DirectLists[i];
            CreateCommandList(&list, CommandListType::Direct);
        }

        for (u32 i = 0; i < m_CommandListPool.m_ComputeLists.count; i++)
        {
            VKCommandList &list = m_CommandListPool.m_ComputeLists[i];
            CreateCommandList(&list, CommandListType::Compute);
        }

        for (u32 i = 0; i < m_CommandListPool.m_CopyLists.count; i++)
        {
            VKCommandList &list = m_CommandListPool.m_CopyLists[i];
            CreateCommandList(&list, CommandListType::Copy);
        }

        /// But for now, we are using frames in flight
        for (auto &list : m_ListsInFlight)
        {
            CreateCommandList(&list, CommandListType::Direct);
        }

        BeginCommandList(GetCurrentCommandList());

        return true;
    }

    void VKAPI::CreateCommandQueue(VKCommandQueue *pHandle, CommandListType type)
    {
        ZoneScoped;

        VkQueue pQueue = nullptr;
        VkFence pFence = CreateFence(false);

        vkGetDeviceQueue(m_pDevice, m_QueueIndexes[(u32)type], 0, &pQueue);

        pHandle->Init(pQueue, pFence);
    }

    void VKAPI::CreateCommandAllocator(VKCommandAllocator *pAllocator, CommandListType type)
    {
        ZoneScoped;

        VkCommandPool &pPool = pAllocator->pHandle;

        VkCommandPoolCreateInfo allocatorInfo = {};
        allocatorInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        allocatorInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        allocatorInfo.queueFamilyIndex = m_QueueIndexes[(u32)type];

        vkCreateCommandPool(m_pDevice, &allocatorInfo, nullptr, &pPool);
    }

    void VKAPI::CreateCommandList(VKCommandList *pHandle, CommandListType type)
    {
        ZoneScoped;

        VkCommandBuffer pList = nullptr;
        VKCommandAllocator &allocator = pHandle->m_Allocator;
        VkFence pFence = CreateFence(false);

        CreateCommandAllocator(&allocator, type);

        VkCommandBufferAllocateInfo listInfo = {};
        listInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        listInfo.commandBufferCount = 1;
        listInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        listInfo.commandPool = allocator.pHandle;

        vkAllocateCommandBuffers(m_pDevice, &listInfo, &pList);

        pHandle->Init(pList, type, pFence);
    }

    VKCommandList *VKAPI::GetCommandList()
    {
        ZoneScoped;

        return m_CommandListPool.Request(CommandListType::Direct);
    }

    VKCommandList *VKAPI::GetCurrentCommandList()
    {
        ZoneScoped;

        return &m_ListsInFlight[m_SwapChain.m_CurrentFrame];
    }

    void VKAPI::Resize(u32 width, u32 height)
    {
        ZoneScoped;
    }

    void VKAPI::CreateSwapChain(VkSwapchainKHR &pHandle, VkSwapchainCreateInfoKHR &info)
    {
        ZoneScoped;

        VkResult vkResult;
        VKCall(vkCreateSwapchainKHR(m_pDevice, &info, nullptr, &pHandle), "Failed to create Vulkan SwapChain.");
    }

    void VKAPI::GetSwapChainImages(VkSwapchainKHR &pHandle, u32 bufferCount, VkImage *pImages)
    {
        ZoneScoped;

        vkGetSwapchainImagesKHR(m_pDevice, pHandle, &bufferCount, pImages);
    }

    void VKAPI::BeginCommandList(VKCommandList *pList)
    {
        ZoneScoped;

        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        vkBeginCommandBuffer(pList->m_pHandle, &beginInfo);
    }

    void VKAPI::EndCommandList(VKCommandList *pList)
    {
        ZoneScoped;

        vkEndCommandBuffer(pList->m_pHandle);
    }

    void VKAPI::ResetCommandAllocator(VKCommandAllocator *pAllocator)
    {
        ZoneScoped;

        vkResetCommandPool(m_pDevice, pAllocator->pHandle, 0);
    }

    void VKAPI::ExecuteCommandList(VKCommandList *pList)
    {
        ZoneScoped;

        VkQueue &pQueueHandle = m_DirectQueue.m_pHandle;
        VkFence &pFence = pList->m_pFence;

        VkSubmitInfo2 submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;

        VkCommandBufferSubmitInfo commandBufferInfo = {};
        commandBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
        commandBufferInfo.commandBuffer = pList->m_pHandle;

        submitInfo.commandBufferInfoCount = 1;
        submitInfo.pCommandBufferInfos = &commandBufferInfo;

        vkQueueSubmit2(pQueueHandle, 1, &submitInfo, pFence);

        vkWaitForFences(m_pDevice, 1, &pFence, false, UINT64_MAX);
        vkResetFences(m_pDevice, 1, &pFence);

        m_CommandListPool.Discard(pList);
    }

    void VKAPI::ExecuteCommandQueue()
    {
        ZoneScoped;

        VkQueue &pQueueHandle = m_DirectQueue.m_pHandle;

        SwapChainFrame *pCurrentFrame = m_SwapChain.GetCurrentFrame();
        SwapChainFrame *pNextFrame = m_SwapChain.GetNextFrame();
        VKCommandList *pList = GetCurrentCommandList();

        VkSemaphore &pAcquireSemp = pCurrentFrame->pAcquireSemp;
        VkSemaphore &pPresentSemp = pCurrentFrame->pPresentSemp;
        VkFence &pCurrentFence = pCurrentFrame->pFence;
        VkFence &pNextFence = pNextFrame->pFence;

        vkWaitForFences(m_pDevice, 1, &pNextFence, false, UINT64_MAX);
        vkResetFences(m_pDevice, 1, &pNextFence);

        VkSubmitInfo2 submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;

        VkCommandBufferSubmitInfo commandBufferInfo = {};
        commandBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
        commandBufferInfo.commandBuffer = pList->m_pHandle;

        submitInfo.commandBufferInfoCount = 1;
        submitInfo.pCommandBufferInfos = &commandBufferInfo;

        VkSemaphoreSubmitInfo acquireSemaphoreInfo = {};
        acquireSemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        acquireSemaphoreInfo.semaphore = pAcquireSemp;
        acquireSemaphoreInfo.stageMask = m_DirectQueue.m_AcquireStage;

        VkSemaphoreSubmitInfo presentSemaphoreInfo = {};
        presentSemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        presentSemaphoreInfo.semaphore = pPresentSemp;
        presentSemaphoreInfo.stageMask = m_DirectQueue.m_PresentStage;

        submitInfo.waitSemaphoreInfoCount = 1;
        submitInfo.pWaitSemaphoreInfos = &acquireSemaphoreInfo;

        submitInfo.signalSemaphoreInfoCount = 1;
        submitInfo.pSignalSemaphoreInfos = &presentSemaphoreInfo;

        vkQueueSubmit2(pQueueHandle, 1, &submitInfo, pCurrentFence);
    }

    VkFence VKAPI::CreateFence(bool signaled)
    {
        ZoneScoped;

        VkFence pHandle = nullptr;

        VkFenceCreateInfo fenceInfo = {};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = signaled;

        vkCreateFence(m_pDevice, &fenceInfo, nullptr, &pHandle);

        return pHandle;
    }

    VkSemaphore VKAPI::CreateSemaphore(u32 initialValue, bool binary)
    {
        ZoneScoped;

        VkSemaphoreTypeCreateInfo semaphoreTypeInfo = {};
        semaphoreTypeInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
        semaphoreTypeInfo.semaphoreType = binary ? VK_SEMAPHORE_TYPE_BINARY : VK_SEMAPHORE_TYPE_TIMELINE;
        semaphoreTypeInfo.initialValue = initialValue;

        VkSemaphoreCreateInfo semaphoreInfo = {};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        semaphoreInfo.pNext = &semaphoreTypeInfo;

        VkSemaphore pHandle;

        vkCreateSemaphore(m_pDevice, &semaphoreInfo, nullptr, &pHandle);

        return pHandle;
    }

    VkRenderPass VKAPI::CreateRenderPass(RenderPassDesc *pDesc)
    {
        ZoneScoped;

        VkRenderPass pHandle = nullptr;

        RenderPassSubpassDesc *pSubpassDesc = pDesc->pSubpassDesc;
        VkSubpassDescription subpassDescVK = {};

        VkSubpassDependency ppSubpassDeps[2] = {};
        VkAttachmentDescription ppAttachmentDescs[kMaxColorAttachmentCount] = {};
        VkAttachmentReference ppAttachmentRefs[kMaxColorAttachmentCount] = {};

        VkAttachmentDescription depthAttachment = {};
        VkAttachmentReference depthAttachmentRef = {};
        depthAttachmentRef.attachment = VK_ATTACHMENT_UNUSED;

        for (u32 i = 0; i < pSubpassDesc->ColorAttachmentCount; i++)
        {
            RenderPassAttachment *&pColorAttachment = pDesc->pSubpassDesc->ppColorAttachments[i];
            VkAttachmentReference &attachmentRef = ppAttachmentRefs[i];
            VkAttachmentDescription &attachment = ppAttachmentDescs[i];

            // Setup reference
            attachmentRef.attachment = i;
            attachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            // Setup actual info
            attachment.flags = 0;
            attachment.format = ToVulkanFormat(pColorAttachment->Format);  // TODO
            attachment.samples = pColorAttachment->SampleCount;

            attachment.loadOp = pColorAttachment->LoadOp;
            attachment.storeOp = pColorAttachment->StoreOp;

            // no stencil for color attachments
            attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

            attachment.initialLayout = pColorAttachment->InitialLayout;
            attachment.finalLayout = pColorAttachment->FinalLayout;
        }

        RenderPassAttachment *&pDepthAttachmentDesc = pSubpassDesc->pDepthAttachment;
        if (pDepthAttachmentDesc)
        {
            VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;
            VkAttachmentLoadOp stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            VkAttachmentStoreOp stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

            if (pDepthAttachmentDesc->Flags & AttachmentFlags::AllowStencil)
            {
                depthFormat = VK_FORMAT_D24_UNORM_S8_UINT;
                stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
                stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
            }

            depthAttachmentRef.attachment = pSubpassDesc->ColorAttachmentCount;
            depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

            depthAttachment.flags = 0;
            depthAttachment.format = depthFormat;
            depthAttachment.samples = pDepthAttachmentDesc->SampleCount;

            depthAttachment.loadOp = pDepthAttachmentDesc->LoadOp;
            depthAttachment.storeOp = pDepthAttachmentDesc->StoreOp;

            depthAttachment.stencilLoadOp = stencilLoadOp;
            depthAttachment.stencilStoreOp = stencilStoreOp;

            depthAttachment.initialLayout = pDepthAttachmentDesc->InitialLayout;
            depthAttachment.finalLayout = pDepthAttachmentDesc->FinalLayout;
        }

        // TODO: Multiple subpasses
        // Setup subpass
        subpassDescVK.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpassDescVK.colorAttachmentCount = pSubpassDesc->ColorAttachmentCount;
        subpassDescVK.pColorAttachments = ppAttachmentRefs;
        subpassDescVK.pDepthStencilAttachment = &depthAttachmentRef;

        // Setup subpass dependency
        ppSubpassDeps[0].srcSubpass = VK_SUBPASS_EXTERNAL;
        ppSubpassDeps[0].dstSubpass = 0;

        ppSubpassDeps[0].srcStageMask = m_DirectQueue.m_AcquireStage;  // Command Queue's stage mask
        ppSubpassDeps[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

        ppSubpassDeps[0].srcAccessMask = 0;
        ppSubpassDeps[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        ppSubpassDeps[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        ////////////////

        ppSubpassDeps[1].srcSubpass = 0;
        ppSubpassDeps[1].dstSubpass = VK_SUBPASS_EXTERNAL;

        ppSubpassDeps[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        ppSubpassDeps[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;  // Don't block ongoing task

        ppSubpassDeps[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        ppSubpassDeps[1].dstAccessMask = 0;

        ppSubpassDeps[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        // Actual renderpass info
        VkRenderPassCreateInfo renderPassInfo = {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;

        renderPassInfo.flags = 0;

        renderPassInfo.attachmentCount = pSubpassDesc->ColorAttachmentCount;
        renderPassInfo.pAttachments = ppAttachmentDescs;

        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpassDescVK;

        renderPassInfo.dependencyCount = 2;  // TODO: Multiple subpasses
        renderPassInfo.pDependencies = ppSubpassDeps;

        vkCreateRenderPass(m_pDevice, &renderPassInfo, nullptr, &pHandle);

        return pHandle;
    }

    VkPipelineCache VKAPI::CreatePipelineCache(u32 initialDataSize, void *pInitialData)
    {
        ZoneScoped;

        VkPipelineCache pHandle = nullptr;

        VkPipelineCacheCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
        createInfo.initialDataSize = initialDataSize;
        createInfo.pInitialData = pInitialData;

        vkCreatePipelineCache(m_pDevice, &createInfo, nullptr, &pHandle);

        return pHandle;
    }

    void VKAPI::BeginPipelineBuildInfo(VKGraphicsPipelineBuildInfo &buildInfo, VkRenderPass pRenderPass)
    {
        ZoneScoped;

        m_PipelineManager.InitBuildInfo(buildInfo, pRenderPass);
    }

    void VKAPI::EndPipelineBuildInfo(VKGraphicsPipelineBuildInfo &buildInfo,
                                     VKPipeline *pTargetHandle,
                                     const std::initializer_list<VKDescriptorSet *> &sets)
    {
        ZoneScoped;

        VkPipeline &pHandle = pTargetHandle->pHandle;
        VkPipelineLayout &pLayout = pTargetHandle->pLayout;

        u32 idx = 0;
        VkDescriptorSetLayout pSetLayouts[8];
        for (auto &pSet : sets)
        {
            pSetLayouts[idx] = pSet->pSetLayoutHandle;
        }

        VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
        pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutCreateInfo.setLayoutCount = sets.size();
        pipelineLayoutCreateInfo.pSetLayouts = pSetLayouts;

        vkCreatePipelineLayout(m_pDevice, &pipelineLayoutCreateInfo, nullptr, &pLayout);

        buildInfo.m_CreateInfo.layout = pLayout;
        buildInfo.m_CreateInfo.renderPass = buildInfo.m_pSetRenderPass;

        vkCreateGraphicsPipelines(m_pDevice, m_PipelineManager.m_pPipelineCache, 1, &buildInfo.m_CreateInfo, nullptr, &pHandle);
    }

    void VKAPI::Frame()
    {
        ZoneScoped;

        VkSwapchainKHR &pSwapChain = m_SwapChain.m_pHandle;

        VkQueue &pQueueHandle = m_DirectQueue.m_pHandle;

        SwapChainFrame *pCurrentFrame = m_SwapChain.GetCurrentFrame();
        VkSemaphore &pAcquireSemp = pCurrentFrame->pAcquireSemp;
        VkSemaphore &pPresentSemp = pCurrentFrame->pPresentSemp;

        u32 imageIndex;
        vkAcquireNextImageKHR(m_pDevice, pSwapChain, UINT64_MAX, pAcquireSemp, nullptr, &imageIndex);

        m_DirectQueue.SetSemaphoreStage(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);

        EndCommandList(GetCurrentCommandList());
        ExecuteCommandQueue();

        VkPresentInfoKHR presentInfo = {};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &pPresentSemp;

        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &pSwapChain;

        presentInfo.pImageIndices = &imageIndex;

        vkQueuePresentKHR(pQueueHandle, &presentInfo);

        // There is a possible chance of that image ordering can be wrong
        // We cannot predict the next image index
        m_SwapChain.NextFrame();
        // m_SwapChain.m_CurrentBuffer = imageIndex;

        BeginCommandList(GetCurrentCommandList());
    }

    VkShaderModule VKAPI::CreateShaderModule(BufferReadStream &buf)
    {
        ZoneScoped;

        VkShaderModule pHandle = nullptr;

        VkShaderModuleCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = buf.GetLength();
        createInfo.pCode = (u32 *)buf.GetData();

        vkCreateShaderModule(m_pDevice, &createInfo, nullptr, &pHandle);

        return pHandle;
    }

    void VKAPI::CreateDescriptorSetLayout(VKDescriptorSet *pSet, const std::initializer_list<VKDescriptorBindingDesc> &layouts)
    {
        ZoneScoped;

        VkDescriptorSetLayoutBinding pBindings[8];

        u32 idx = 0;
        for (auto &element : layouts)
        {
            pBindings[idx].binding = element.BindingID;
            pBindings[idx].descriptorType = ToVulkanDescriptorType(element.Type);
            pBindings[idx].stageFlags = element.ShaderStageFlags;
            pBindings[idx].descriptorCount = element.ArraySize;

            idx++;
        }

        // First we need layout info
        VkDescriptorSetLayoutCreateInfo layoutCreateInfo = {};
        layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutCreateInfo.bindingCount = idx;
        layoutCreateInfo.pBindings = pBindings;

        vkCreateDescriptorSetLayout(m_pDevice, &layoutCreateInfo, nullptr, &pSet->pSetLayoutHandle);

        // And the actual set
        VkDescriptorSetAllocateInfo allocateCreateInfo = {};
        allocateCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocateCreateInfo.descriptorPool = m_pDescriptorPool;
        allocateCreateInfo.descriptorSetCount = 1;
        allocateCreateInfo.pSetLayouts = &pSet->pSetLayoutHandle;

        vkAllocateDescriptorSets(m_pDevice, &allocateCreateInfo, &pSet->pHandle);
    }

    VkDescriptorPool VKAPI::CreateDescriptorPool(const std::initializer_list<VKDescriptorBindingDesc> &layouts)
    {
        ZoneScoped;

        VkDescriptorPool pHandle = nullptr;
        VkDescriptorPoolSize pPoolSizes[(u32)DescriptorType::Count];

        u32 maxSets = 0;
        u32 idx = 0;
        for (auto &element : layouts)
        {
            pPoolSizes[idx].type = ToVulkanDescriptorType(element.Type);
            pPoolSizes[idx].descriptorCount = element.ArraySize;
            maxSets += element.ArraySize;

            idx++;
        }

        VkDescriptorPoolCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        createInfo.maxSets = maxSets;
        createInfo.poolSizeCount = idx;
        createInfo.pPoolSizes = pPoolSizes;

        vkCreateDescriptorPool(m_pDevice, &createInfo, nullptr, &pHandle);

        return pHandle;
    }

    void VKAPI::CreateImage(VKImage *pHandle, ImageDesc *pDesc, ImageData *pData)
    {
        ZoneScoped;

        pHandle->Init(pDesc, pData);

        VkImage &pImage = pHandle->m_pHandle;

        VkImageCreateInfo imageCreateInfo = {};
    }

    void VKAPI::CreateImageView(VKImage *pHandle, ResourceType type, ResourceFormat format, u32 mipCount)
    {
        ZoneScoped;

        VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;

        if (type == ResourceType::Depth)
        {
            aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        }

        VkImageViewCreateInfo imageViewCreateInfo = {};
        imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;

        imageViewCreateInfo.image = pHandle->m_pHandle;
        imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        imageViewCreateInfo.format = ToVulkanFormat(format);

        imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

        imageViewCreateInfo.subresourceRange.aspectMask = aspectFlags;
        imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
        imageViewCreateInfo.subresourceRange.levelCount = 1;
        imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
        imageViewCreateInfo.subresourceRange.layerCount = 1;

        vkCreateImageView(m_pDevice, &imageViewCreateInfo, nullptr, &pHandle->m_pViewHandle);
    }

    VkFramebuffer VKAPI::CreateFramebuffer(XMUINT2 size, u32 attachmentCount, VKImage *pAttachments, VkRenderPass &pRenderPass)
    {
        ZoneScoped;

        VkFramebuffer pHandle = nullptr;

        VkImageView pNativeHandles[8];

        for (u32 i = 0; i < attachmentCount; i++)
        {
            pNativeHandles[i] = pAttachments[i].m_pViewHandle;
        }

        VkFramebufferCreateInfo framebufferCreateInfo = {};
        framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;

        framebufferCreateInfo.renderPass = pRenderPass;

        framebufferCreateInfo.attachmentCount = attachmentCount;
        framebufferCreateInfo.pAttachments = pNativeHandles;

        framebufferCreateInfo.width = size.x;
        framebufferCreateInfo.height = size.y;
        framebufferCreateInfo.layers = 1;

        vkCreateFramebuffer(m_pDevice, &framebufferCreateInfo, nullptr, &pHandle);

        return pHandle;
    }

    bool VKAPI::IsFormatSupported(VkFormat format, VkColorSpaceKHR *pColorSpaceOut)
    {
        ZoneScoped;

        for (u32 i = 0; i < m_ValidSurfaceFormatCount; i++)
        {
            VkSurfaceFormatKHR &surfFormat = m_pValidSurfaceFormats[i];

            if (surfFormat.format == format)
            {
                if (pColorSpaceOut)
                    *pColorSpaceOut = surfFormat.colorSpace;

                return true;
            }
        }

        return false;
    }

    bool VKAPI::IsPresentModeSupported(VkPresentModeKHR mode)
    {
        ZoneScoped;

        for (u32 i = 0; i < m_ValidSurfaceFormatCount; i++)
        {
            VkPresentModeKHR &presentMode = m_pValidPresentModes[i];

            if (presentMode == mode)
            {
                return true;
            }
        }

        return false;
    }

    void VKAPI::GetSurfaceCapabilities(VkSurfaceCapabilitiesKHR &capabilitiesOut)
    {
        ZoneScoped;

        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_pPhysicalDevice, m_pSurface, &capabilitiesOut);
    }

    bool VKAPI::LoadVulkan()
    {
        m_VulkanLib = LoadLibraryA("vulkan-1.dll");

        if (m_VulkanLib == NULL)
        {
            LOG_CRITICAL("Failed to find Vulkan dynamic library.");
            return false;
        }

#define _VK_DEFINE_FUNCTION(_name)                            \
    _name = (PFN_##_name)GetProcAddress(m_VulkanLib, #_name); \
    if (_name == nullptr)                                     \
    {                                                         \
        LOG_CRITICAL("Cannot load vulkan function `{}`!");    \
    }                                                         \
    // LOG_TRACE("Loading Vulkan function: {}:{}", #_name, (void *)_name);

        _VK_IMPORT_SYMBOLS
#undef _VK_DEFINE_FUNCTION

        return true;
    }

    bool VKAPI::SetupInstance(HWND windowHandle)
    {
        ZoneScoped;

        VkResult vkResult;

#ifdef _DEBUG

        /// Setup Layers
        u32 layerCount = 0;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

        VkLayerProperties *ppLayerProps = new VkLayerProperties[layerCount];
        vkEnumerateInstanceLayerProperties(&layerCount, ppLayerProps);

        constexpr eastl::array<const char *, 3> ppRequiredLayers = {
            "VK_LAYER_LUNARG_monitor",
            "VK_LAYER_KHRONOS_validation",
            "VK_LAYER_KHRONOS_synchronization2",
        };

        for (auto &pLayerName : ppRequiredLayers)
        {
            bool found = false;
            eastl::string_view layerSV(pLayerName);

            for (u32 i = 0; i < layerCount; i++)
            {
                VkLayerProperties &prop = ppLayerProps[i];

                if (layerSV == prop.layerName)
                {
                    found = true;

                    continue;
                }
            }

            if (found)
                continue;

            LOG_ERROR("Following layer is not found in this instance: {}", layerSV);

            return false;
        }

        delete[] ppLayerProps;

#endif

        /// Setup Extensions

        /// Required extensions
        // * VK_KHR_surface
        // * VK_KHR_get_physical_device_properties2
        // * VK_KHR_win32_surface
        // Debug only
        // * VK_EXT_debug_utils
        // * VK_EXT_debug_report

        u32 instanceExtensionCount = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtensionCount, nullptr);

        VkExtensionProperties *ppExtensionProp = new VkExtensionProperties[instanceExtensionCount];
        vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtensionCount, ppExtensionProp);

#ifdef _DEBUG
        constexpr u32 debugExtensionCount = 2;
#else
        constexpr u32 debugExtensionCount = 0;
#endif

        constexpr eastl::array<const char *, 3 + debugExtensionCount> ppRequiredExtensions = {
            "VK_KHR_surface",
            "VK_KHR_get_physical_device_properties2",
            "VK_KHR_win32_surface",

            //! Debug extensions, always put it to bottom
            "VK_EXT_debug_utils",
            "VK_EXT_debug_report",
        };

        for (auto &pExtensionName : ppRequiredExtensions)
        {
            bool found = false;
            eastl::string_view extensionSV(pExtensionName);

            for (u32 i = 0; i < instanceExtensionCount; i++)
            {
                VkExtensionProperties &prop = ppExtensionProp[i];

                if (extensionSV == prop.extensionName)
                {
                    found = true;

                    continue;
                }
            }

            if (found)
                continue;

            LOG_ERROR("Following extension is not found in this instance: {}", extensionSV);

            return false;
        }

        delete[] ppExtensionProp;

        /// Initialize Instance

        VkApplicationInfo appInfo = {};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Lorr";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "Lorr Rendering Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_3;

        VkInstanceCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;
        createInfo.enabledExtensionCount = ppRequiredExtensions.count;
        createInfo.ppEnabledExtensionNames = ppRequiredExtensions.data();

#ifdef _DEBUG
        createInfo.enabledLayerCount = ppRequiredLayers.count;
        createInfo.ppEnabledLayerNames = ppRequiredLayers.data();
#endif

        /// Initialize surface

        VkWin32SurfaceCreateInfoKHR surfaceInfo = {};
        surfaceInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        surfaceInfo.hwnd = windowHandle;
        surfaceInfo.hinstance = GetModuleHandleA(NULL);  // TODO

        VKCallRet(vkCreateInstance(&createInfo, nullptr, &m_pInstance), "Failed to create Vulkan Instance.", false);
        VKCallRet(vkCreateWin32SurfaceKHR(m_pInstance, &surfaceInfo, nullptr, &m_pSurface), "Failed to create Vulkan Win32 surface.", false);

        return true;
    }

    bool VKAPI::SetupQueues(VkPhysicalDevice &pPhysicalDevice, VkPhysicalDeviceFeatures &features)
    {
        ZoneScoped;

        u32 queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(pPhysicalDevice, &queueFamilyCount, nullptr);

        VkQueueFamilyProperties *pQueueFamilyProps = new VkQueueFamilyProperties[queueFamilyCount];
        vkGetPhysicalDeviceQueueFamilyProperties(pPhysicalDevice, &queueFamilyCount, pQueueFamilyProps);

        u32 foundGraphicsFamilyIndex = -1;
        u32 foundComputeFamilyIndex = -1;
        u32 foundTransferFamilyIndex = -1;

        constexpr u32 kFullQueueFlags = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT;

        for (u32 i = 0; i < queueFamilyCount; i++)
        {
            VkQueueFamilyProperties &props = pQueueFamilyProps[i];

            if (foundGraphicsFamilyIndex == -1 && props.queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                VkBool32 presentSupported = false;
                vkGetPhysicalDeviceSurfaceSupportKHR(pPhysicalDevice, i, m_pSurface, &presentSupported);

                if (!presentSupported)
                {
                    LOG_ERROR("Graphics queue doesn't support presentantion, abort.");

                    break;
                }

                m_AvailableQueueCount++;
                foundGraphicsFamilyIndex = i;

                continue;
            }

            constexpr u32 kComputeFilter = VK_QUEUE_GRAPHICS_BIT;
            if (foundComputeFamilyIndex == -1 && props.queueFlags & VK_QUEUE_COMPUTE_BIT && (props.queueFlags & kComputeFilter) == 0)
            {
                m_AvailableQueueCount++;
                foundComputeFamilyIndex = i;

                continue;
            }

            constexpr u32 kTransferFilter = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;
            if (foundTransferFamilyIndex == -1 && props.queueFlags & VK_QUEUE_TRANSFER_BIT && (props.queueFlags & kTransferFilter) == 0)
            {
                m_AvailableQueueCount++;
                foundTransferFamilyIndex = i;

                continue;
            }

            break;
        }

        if (foundGraphicsFamilyIndex == -1)
        {
            LOG_CRITICAL("Could not find any queue families matched our requirements.");
            return false;
        }

        if (foundComputeFamilyIndex == -1)
        {
            for (u32 i = 0; i < queueFamilyCount; i++)
            {
                VkQueueFamilyProperties &props = pQueueFamilyProps[i];

                if (props.queueFlags & VK_QUEUE_COMPUTE_BIT)
                {
                    foundComputeFamilyIndex = i;
                    break;
                }
            }

            return false;
        }

        if (foundTransferFamilyIndex == -1)
        {
            for (u32 i = 0; i < queueFamilyCount; i++)
            {
                VkQueueFamilyProperties &props = pQueueFamilyProps[i];

                if (props.queueFlags & VK_QUEUE_TRANSFER_BIT)
                {
                    foundTransferFamilyIndex = i;
                    break;
                }
            }

            return false;
        }

        delete[] pQueueFamilyProps;

        m_QueueIndexes[(u32)CommandListType::Direct] = foundGraphicsFamilyIndex;
        m_QueueIndexes[(u32)CommandListType::Compute] = foundComputeFamilyIndex;
        m_QueueIndexes[(u32)CommandListType::Copy] = foundTransferFamilyIndex;

        return true;
    }

    bool VKAPI::SetupDevice(VkPhysicalDevice &pPhysicalDevice, VkPhysicalDeviceFeatures &features)
    {
        ZoneScoped;

        constexpr float kHighPrior = 1.0;

        VkDeviceQueueCreateInfo pQueueInfos[3] = {};
        for (u32 i = 0; i < m_AvailableQueueCount; i++)
        {
            pQueueInfos[i].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            pQueueInfos[i].queueFamilyIndex = m_QueueIndexes[i];
            pQueueInfos[i].queueCount = 1;
            pQueueInfos[i].pQueuePriorities = &kHighPrior;
        }

        /// Get device extensions

        u32 deviceExtensionCount = 0;
        vkEnumerateDeviceExtensionProperties(pPhysicalDevice, nullptr, &deviceExtensionCount, nullptr);

        VkExtensionProperties *pDeviceExtensions = new VkExtensionProperties[deviceExtensionCount];
        vkEnumerateDeviceExtensionProperties(pPhysicalDevice, nullptr, &deviceExtensionCount, pDeviceExtensions);

        constexpr eastl::array<const char *, 3> ppRequiredExtensions = {
            "VK_KHR_swapchain",
            "VK_KHR_create_renderpass2",
            "VK_KHR_synchronization2",
        };

        for (auto &pExtensionName : ppRequiredExtensions)
        {
            bool found = false;
            eastl::string_view extensionSV(pExtensionName);

            for (u32 i = 0; i < deviceExtensionCount; i++)
            {
                VkExtensionProperties &prop = pDeviceExtensions[i];

                if (extensionSV == prop.extensionName)
                {
                    found = true;

                    continue;
                }
            }

            if (found)
                continue;

            LOG_ERROR("Following extension is not found in this device: {}", extensionSV);

            return false;
        }

        ////////////////////////////////////
        /// Device Feature Configuration ///
        ////////////////////////////////////

        // * Vulkan 1.1
        VkPhysicalDeviceVulkan11Features vk11Features = {};
        vk11Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;

        // * Vulkan 1.2
        VkPhysicalDeviceVulkan12Features vk12Features = {};
        vk12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
        vk12Features.pNext = &vk11Features;

        // * Vulkan 1.3
        VkPhysicalDeviceVulkan13Features vk13Features = {};
        vk13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
        vk12Features.pNext = &vk12Features;

        // Enables `VK_KHR_synchronization2`
        vk13Features.synchronization2 = true;

        VkDeviceCreateInfo deviceCreateInfo = {};
        deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

        deviceCreateInfo.pNext = &vk13Features;

        deviceCreateInfo.queueCreateInfoCount = m_AvailableQueueCount;
        deviceCreateInfo.pQueueCreateInfos = pQueueInfos;
        deviceCreateInfo.enabledExtensionCount = ppRequiredExtensions.count;
        deviceCreateInfo.ppEnabledExtensionNames = &ppRequiredExtensions[0];

        if (vkCreateDevice(pPhysicalDevice, &deviceCreateInfo, nullptr, &m_pDevice) != VK_SUCCESS)
        {
            LOG_CRITICAL("Failed to create Vulkan device. ");

            return false;
        }

        return true;
    }

    /// ---------------- Type Conversion LUTs ----------------

    constexpr VkFormat kFormatLUT[] = {
        VK_FORMAT_UNDEFINED,
        VK_FORMAT_BC1_RGBA_UNORM_BLOCK,  // BC1
        VK_FORMAT_B8G8R8A8_UNORM,        // RGBA8
        VK_FORMAT_B8G8R8A8_SRGB,         // RGBA8_SRGB
        VK_FORMAT_R32G32B32A32_SFLOAT,   // RGBA32F
        VK_FORMAT_R32_UINT,              // R32U
        VK_FORMAT_R32_SFLOAT,            // R32F
        VK_FORMAT_D32_SFLOAT,            // D32F
        VK_FORMAT_D24_UNORM_S8_UINT,     // D24FS8U

    };

    constexpr VkFormat kAttribFormatLUT[] = {
        VK_FORMAT_UNDEFINED,
        VK_FORMAT_R32_SFLOAT,               // Float
        VK_FORMAT_R32G32_SFLOAT,            // Vec2
        VK_FORMAT_R32G32B32_SFLOAT,         // Vec3
        VK_FORMAT_B10G11R11_UFLOAT_PACK32,  // Vec3PK
        VK_FORMAT_R32G32B32A32_SFLOAT,      // Vec4
        VK_FORMAT_R32_UINT,                 // UInt

    };

    constexpr VkPrimitiveTopology kPrimitiveLUT[] = {
        VK_PRIMITIVE_TOPOLOGY_POINT_LIST,      // PointList
        VK_PRIMITIVE_TOPOLOGY_LINE_LIST,       // LineList
        VK_PRIMITIVE_TOPOLOGY_LINE_STRIP,      // LineStrip
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,   // TriangleList
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,  // TriangleStrip
        VK_PRIMITIVE_TOPOLOGY_PATCH_LIST,      // Patch
    };

    constexpr VkCullModeFlags kCullModeLUT[] = {
        VK_CULL_MODE_FRONT_AND_BACK,  // None
        VK_CULL_MODE_FRONT_BIT,       // Front
        VK_CULL_MODE_BACK_BIT,        // Back
    };

    constexpr VkDescriptorType kDescriptorTypeLUT[] = {
        VK_DESCRIPTOR_TYPE_SAMPLER,         // Sampler
        VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,   // Texture
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,  // ConstantBuffer
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,  // StructuredBuffer
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,   // RWTexture
    };

    VkFormat VKAPI::ToVulkanFormat(ResourceFormat format)
    {
        return kFormatLUT[(u32)format];
    }

    VkFormat VKAPI::ToVulkanFormat(VertexAttribType format)
    {
        return kAttribFormatLUT[(u32)format];
    }

    VkPrimitiveTopology VKAPI::ToVulkanTopology(PrimitiveType type)
    {
        return kPrimitiveLUT[(u32)type];
    }

    VkCullModeFlags VKAPI::ToVulkanCullMode(CullMode mode)
    {
        return kCullModeLUT[(u32)mode];
    }

    VkDescriptorType VKAPI::ToVulkanDescriptorType(DescriptorType type)
    {
        return kDescriptorTypeLUT[(u32)type];
    }

}  // namespace lr::g