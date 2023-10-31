#include "Device.hh"

#include "Core/Config.hh"
#include "Memory/MemoryUtils.hh"

#include "Shader.hh"

namespace lr::Graphics
{
bool Device::Init(PhysicalDevice *pPhysicalDevice, VkDevice pHandle)
{
    ZoneScoped;

    LOG_TRACE("Initializing device...");

    m_pHandle = pHandle;

    if (!VK::LoadVulkanDevice(m_pHandle))
    {
        LOG_ERROR("Failed to initialize device!");
        return false;
    }

    m_pPipelineCache = CreatePipelineCache();
    m_pTracyCtx = TracyVkContextHostCalibrated(pPhysicalDevice->m_pHandle, m_pHandle);

    LOG_TRACE("Successfully initialized device.");

    return true;
}

CommandQueue *Device::CreateCommandQueue(CommandType type, u32 queueIndex)
{
    ZoneScoped;

    CommandQueue *pQueue = new CommandQueue;
    pQueue->m_Type = type;
    pQueue->m_QueueIndex = queueIndex;
    vkGetDeviceQueue(m_pHandle, queueIndex, 0, &pQueue->m_pHandle);

    return pQueue;
}

CommandAllocator *Device::CreateCommandAllocator(CommandQueue *pQueue, bool resetLists)
{
    ZoneScoped;

    CommandAllocator *pAllocator = new CommandAllocator;
    pAllocator->m_Type = pQueue->m_Type;
    pAllocator->m_pQueue = pQueue;

    VkCommandPoolCreateFlags flags = 0;
    if (resetLists)
        flags |= VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    VkCommandPoolCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = flags,
        .queueFamilyIndex = pQueue->m_QueueIndex,
    };
    vkCreateCommandPool(m_pHandle, &createInfo, nullptr, &pAllocator->m_pHandle);

    return pAllocator;
}

CommandList *Device::CreateCommandList(CommandAllocator *pAllocator)
{
    ZoneScoped;

    CommandList *pList = new CommandList;
    pList->m_Type = pAllocator->m_Type;
    pList->m_pAllocator = pAllocator;

    VkCommandBufferAllocateInfo listInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = pAllocator->m_pHandle,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    vkAllocateCommandBuffers(m_pHandle, &listInfo, &pList->m_pHandle);

    return pList;
}

void Device::BeginCommandList(CommandList *pList)
{
    ZoneScoped;

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.pNext = nullptr;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(pList->m_pHandle, &beginInfo);
}

void Device::EndCommandList(CommandList *pList)
{
    ZoneScoped;

    TracyVkCollect(m_pTracyCtx, pList->m_pHandle);
    vkEndCommandBuffer(pList->m_pHandle);
}

void Device::ResetCommandAllocator(CommandAllocator *pAllocator)
{
    ZoneScoped;

    vkResetCommandPool(
        m_pHandle, pAllocator->m_pHandle, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT);
}

void Device::Submit(CommandQueue *pQueue, SubmitDesc *pDesc)
{
    ZoneScoped;

    VkSubmitInfo2 submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .waitSemaphoreInfoCount = (u32)pDesc->m_WaitSemas.size(),
        .pWaitSemaphoreInfos = pDesc->m_WaitSemas.data(),
        .commandBufferInfoCount = (u32)pDesc->m_Lists.size(),
        .pCommandBufferInfos = pDesc->m_Lists.data(),
        .signalSemaphoreInfoCount = (u32)pDesc->m_SignalSemas.size(),
        .pSignalSemaphoreInfos = pDesc->m_SignalSemas.begin(),
    };

    vkQueueSubmit2(pQueue->m_pHandle, 1, &submitInfo, nullptr);
}

Fence Device::CreateFence(bool signaled)
{
    ZoneScoped;

    Fence pFence = nullptr;

    VkFenceCreateInfo fenceInfo = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = signaled,
    };
    vkCreateFence(m_pHandle, &fenceInfo, nullptr, &pFence);

    return pFence;
}

void Device::DeleteFence(Fence pFence)
{
    ZoneScoped;

    vkDestroyFence(m_pHandle, pFence, nullptr);
}

bool Device::IsFenceSignaled(Fence pFence)
{
    ZoneScoped;

    return vkGetFenceStatus(m_pHandle, pFence) == VK_SUCCESS;
}

void Device::ResetFence(Fence pFence)
{
    ZoneScoped;

    vkResetFences(m_pHandle, 1, &pFence);
}

Semaphore *Device::CreateBinarySemaphore()
{
    ZoneScoped;

    Semaphore *pSemaphore = new Semaphore;
    pSemaphore->m_Value = 0;

    VkSemaphoreTypeCreateInfo semaphoreTypeInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
        .semaphoreType = VK_SEMAPHORE_TYPE_BINARY,
        .initialValue = pSemaphore->m_Value,
    };

    VkSemaphoreCreateInfo semaphoreInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = &semaphoreTypeInfo,
    };
    vkCreateSemaphore(m_pHandle, &semaphoreInfo, nullptr, &pSemaphore->m_pHandle);

    return pSemaphore;
}

Semaphore *Device::CreateTimelineSemaphore(u64 initialValue)
{
    ZoneScoped;

    Semaphore *pSemaphore = new Semaphore;
    pSemaphore->m_Value = initialValue;

    VkSemaphoreTypeCreateInfo semaphoreTypeInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
        .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
        .initialValue = pSemaphore->m_Value,
    };

    VkSemaphoreCreateInfo semaphoreInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = &semaphoreTypeInfo,
    };
    vkCreateSemaphore(m_pHandle, &semaphoreInfo, nullptr, &pSemaphore->m_pHandle);

    return pSemaphore;
}

void Device::DeleteSemaphore(Semaphore *pSemaphore)
{
    ZoneScoped;

    vkDestroySemaphore(m_pHandle, pSemaphore->m_pHandle, nullptr);
    delete pSemaphore;
}

void Device::WaitForSemaphore(Semaphore *pSemaphore, u64 desiredValue, u64 timeout)
{
    ZoneScoped;

    VkSemaphoreWaitInfo waitInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
        .semaphoreCount = 1,
        .pSemaphores = &pSemaphore->m_pHandle,
        .pValues = &desiredValue,
    };
    vkWaitSemaphores(m_pHandle, &waitInfo, timeout);
}

VkPipelineCache Device::CreatePipelineCache(u32 initialDataSize, void *pInitialData)
{
    ZoneScoped;

    VkPipelineCache pHandle = nullptr;

    VkPipelineCacheCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
        .initialDataSize = initialDataSize,
        .pInitialData = pInitialData,
    };
    vkCreatePipelineCache(m_pHandle, &createInfo, nullptr, &pHandle);

    return pHandle;
}

PipelineLayout Device::CreatePipelineLayout(
    eastl::span<DescriptorSetLayout> layouts, eastl::span<PushConstantDesc> pushConstants)
{
    ZoneScoped;

    PipelineLayout pLayout = nullptr;

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .setLayoutCount = (u32)layouts.size(),
        .pSetLayouts = layouts.data(),
        .pushConstantRangeCount = (u32)pushConstants.size(),
        .pPushConstantRanges = pushConstants.data(),
    };
    vkCreatePipelineLayout(m_pHandle, &pipelineLayoutCreateInfo, nullptr, &pLayout);

    return pLayout;
}

Pipeline *Device::CreateGraphicsPipeline(GraphicsPipelineBuildInfo *pBuildInfo)
{
    ZoneScoped;

    Pipeline *pPipeline = new Pipeline;
    pPipeline->m_BindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    pPipeline->m_pLayout = pBuildInfo->m_pLayout;

    /// BOUND RENDER TARGETS -----------------------------------------------------

    VkPipelineRenderingCreateInfo renderingInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .pNext = nullptr,
        .viewMask = 0,
        .colorAttachmentCount = (u32)pBuildInfo->m_ColorAttachmentFormats.size(),
        .pColorAttachmentFormats =
            (VkFormat *)pBuildInfo->m_ColorAttachmentFormats.data(),
        .depthAttachmentFormat = (VkFormat)pBuildInfo->m_DepthAttachmentFormat,
    };

    /// BOUND SHADER STAGES ------------------------------------------------------

    VkPipelineShaderStageCreateInfo pShaderStageInfos[(u32)ShaderStage::Count] = {};
    for (u32 i = 0; i < pBuildInfo->m_Shaders.size(); i++)
    {
        Shader *pShader = pBuildInfo->m_Shaders[i];

        VkPipelineShaderStageCreateInfo &info = pShaderStageInfos[i];
        info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        info.pNext = nullptr;
        info.stage = (VkShaderStageFlagBits)pShader->m_Type;
        info.module = pShader->m_pHandle;
        info.pName = "main";
    }

    /// INPUT LAYOUT  ------------------------------------------------------------

    VkPipelineVertexInputStateCreateInfo inputLayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pNext = nullptr,
    };

    // VkVertexInputBindingDescription inputBindingInfo = {};
    // VkVertexInputAttributeDescription pAttribInfos[Limits::MaxVertexAttribs] = {};

    /// INPUT ASSEMBLY -----------------------------------------------------------

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .pNext = nullptr,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    };

    /// TESSELLATION -------------------------------------------------------------

    VkPipelineTessellationStateCreateInfo tessellationInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,
        .pNext = nullptr,
        .patchControlPoints = 0,  // TODO
    };

    /// VIEWPORT -----------------------------------------------------------------

    VkPipelineViewportStateCreateInfo viewportInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pNext = nullptr,
        .viewportCount = 1,
        .pViewports = nullptr,
        .scissorCount = 1,
        .pScissors = nullptr,
    };

    /// RASTERIZER ---------------------------------------------------------------

    VkPipelineRasterizationStateCreateInfo rasterizerInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .pNext = nullptr,
        .depthClampEnable = pBuildInfo->m_EnableDepthClamp,
        .polygonMode = (VkPolygonMode)pBuildInfo->m_SetFillMode,
        .cullMode = (VkCullModeFlags)pBuildInfo->m_SetCullMode,
        .frontFace = (VkFrontFace)!pBuildInfo->m_FrontFaceCCW,
        .depthBiasEnable = pBuildInfo->m_EnableDepthBias,
        .depthBiasConstantFactor = pBuildInfo->m_DepthBiasFactor,
        .depthBiasClamp = pBuildInfo->m_DepthBiasClamp,
        .depthBiasSlopeFactor = pBuildInfo->m_DepthSlopeFactor,
        .lineWidth = 1.0,
    };

    /// MULTISAMPLE --------------------------------------------------------------

    VkPipelineMultisampleStateCreateInfo multisampleInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .pNext = nullptr,
        .rasterizationSamples =
            (VkSampleCountFlagBits)(1 << (pBuildInfo->m_MultiSampleBitCount - 1)),
        .alphaToCoverageEnable = pBuildInfo->m_EnableAlphaToCoverage,
        .alphaToOneEnable = false,
    };

    /// DEPTH STENCIL ------------------------------------------------------------

    VkPipelineDepthStencilStateCreateInfo depthStencilInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .pNext = nullptr,
        .depthTestEnable = pBuildInfo->m_EnableDepthTest,
        .depthWriteEnable = pBuildInfo->m_EnableDepthWrite,
        .depthCompareOp = (VkCompareOp)pBuildInfo->m_DepthCompareOp,
        .depthBoundsTestEnable = false,
        .stencilTestEnable = pBuildInfo->m_EnableStencilTest,

        .front.compareOp = (VkCompareOp)pBuildInfo->m_StencilFrontFaceOp.m_CompareFunc,
        .front.depthFailOp = (VkStencilOp)pBuildInfo->m_StencilFrontFaceOp.m_DepthFail,
        .front.failOp = (VkStencilOp)pBuildInfo->m_StencilFrontFaceOp.m_Fail,
        .front.passOp = (VkStencilOp)pBuildInfo->m_StencilFrontFaceOp.m_Pass,

        .back.compareOp = (VkCompareOp)pBuildInfo->m_StencilBackFaceOp.m_CompareFunc,
        .back.depthFailOp = (VkStencilOp)pBuildInfo->m_StencilBackFaceOp.m_DepthFail,
        .back.failOp = (VkStencilOp)pBuildInfo->m_StencilBackFaceOp.m_Fail,
        .back.passOp = (VkStencilOp)pBuildInfo->m_StencilBackFaceOp.m_Pass,
    };

    /// COLOR BLEND --------------------------------------------------------------

    VkPipelineColorBlendStateCreateInfo colorBlendInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .pNext = nullptr,
        .logicOpEnable = false,
        .attachmentCount = (u32)pBuildInfo->m_BlendAttachments.size(),
        .pAttachments = pBuildInfo->m_BlendAttachments.data(),
    };

    /// DYNAMIC STATE ------------------------------------------------------------

    constexpr static eastl::array<VkDynamicState, 2> kDynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamicStateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .pNext = nullptr,
        .dynamicStateCount = kDynamicStates.size(),
        .pDynamicStates = kDynamicStates.data(),
    };

    /// GRAPHICS PIPELINE --------------------------------------------------------

    VkGraphicsPipelineCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &renderingInfo,
        .flags = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT,
        .stageCount = (u32)pBuildInfo->m_Shaders.size(),
        .pStages = pShaderStageInfos,
        .pVertexInputState = &inputLayoutInfo,
        .pInputAssemblyState = &inputAssemblyInfo,
        .pTessellationState = &tessellationInfo,
        .pViewportState = &viewportInfo,
        .pRasterizationState = &rasterizerInfo,
        .pMultisampleState = &multisampleInfo,
        .pDepthStencilState = &depthStencilInfo,
        .pColorBlendState = &colorBlendInfo,
        .pDynamicState = &dynamicStateInfo,
        .layout = pBuildInfo->m_pLayout,
    };

    vkCreateGraphicsPipelines(
        m_pHandle, m_pPipelineCache, 1, &createInfo, nullptr, &pPipeline->m_pHandle);

    return pPipeline;
}

Pipeline *Device::CreateComputePipeline(ComputePipelineBuildInfo *pBuildInfo)
{
    ZoneScoped;

    Pipeline *pPipeline = new Pipeline;
    pPipeline->m_BindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;
    pPipeline->m_pLayout = pBuildInfo->m_pLayout;

    VkPipelineShaderStageCreateInfo shaderStage = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .stage = VK_SHADER_STAGE_COMPUTE_BIT,
        .module = pBuildInfo->m_pShader->m_pHandle,
        .pName = "main",
        .pSpecializationInfo = nullptr,
    };

    VkComputePipelineCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .pNext = nullptr,
        .stage = shaderStage,
        .layout = pBuildInfo->m_pLayout,
    };

    vkCreateComputePipelines(
        m_pHandle, m_pPipelineCache, 1, &createInfo, nullptr, &pPipeline->m_pHandle);

    return pPipeline;
}

SwapChain *Device::CreateSwapChain(Surface *pSurface, SwapChainDesc *pDesc)
{
    ZoneScoped;

    SwapChain *pSwapChain = new SwapChain;
    pSwapChain->m_FrameCount = pDesc->m_FrameCount;
    pSwapChain->m_Width = pDesc->m_Width;
    pSwapChain->m_Height = pDesc->m_Height;
    pSwapChain->m_ImageFormat = pDesc->m_Format;
    pSwapChain->m_ColorSpace = pDesc->m_ColorSpace;
    pSwapChain->m_PresentMode = pDesc->m_PresentMode;

    VkSwapchainCreateInfoKHR swapChainInfo = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .pNext = nullptr,
        .surface = pSurface->m_pHandle,
        .minImageCount = pSwapChain->m_FrameCount,
        .imageFormat = (VkFormat)pSwapChain->m_ImageFormat,
        .imageColorSpace = pSwapChain->m_ColorSpace,
        .imageExtent = { pSwapChain->m_Width, pSwapChain->m_Height },
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,
        .preTransform = pSurface->GetTransform(),
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = pSwapChain->m_PresentMode,
        .clipped = VK_TRUE,
    };

    VkResult result =
        vkCreateSwapchainKHR(m_pHandle, &swapChainInfo, nullptr, &pSwapChain->m_pHandle);
    if (result != VK_SUCCESS)
    {
        LOG_ERROR("Failed to create SwapChain! {}", (u32)result);
        return nullptr;
    }

    return pSwapChain;
}

void Device::DeleteSwapChain(SwapChain *pSwapChain, bool keepSelf)
{
    ZoneScoped;

    vkDestroySwapchainKHR(m_pHandle, pSwapChain->m_pHandle, nullptr);

    if (!keepSelf)
        delete pSwapChain;
}

ls::ref_array<Image *> Device::GetSwapChainImages(SwapChain *pSwapChain)
{
    ZoneScoped;

    u32 imageCount = pSwapChain->m_FrameCount;
    ls::ref_array<Image *> ppImages(new Image *[imageCount]);
    ls::ref_array<VkImage> ppImageHandles(new VkImage[imageCount]);
    vkGetSwapchainImagesKHR(
        m_pHandle, pSwapChain->m_pHandle, &imageCount, ppImageHandles.get());

    for (u32 i = 0; i < imageCount; i++)
    {
        Image *&pImage = ppImages[i];
        pImage = new Image;
        pImage->m_pHandle = ppImageHandles[i];
        pImage->m_Width = pSwapChain->m_Width;
        pImage->m_Height = pSwapChain->m_Height;
        pImage->m_DataSize = ~0;
        pImage->m_DataOffset = ~0;
        pImage->m_MipMapLevels = 1;
        pImage->m_Format = pSwapChain->m_ImageFormat;
        CreateImageView(pImage, ImageUsage::ColorAttachment);

        SetObjectName(pImage, _FMT("Swap Chain Image {}", i));
    }

    return ppImages;
}

void Device::WaitForWork()
{
    ZoneScoped;

    vkDeviceWaitIdle(m_pHandle);
}

u32 Device::AcquireImage(SwapChain *pSwapChain, Semaphore *pSemaphore)
{
    ZoneScoped;

    u32 imageIdx = 0;
    VkResult res = vkAcquireNextImageKHR(
        m_pHandle,
        pSwapChain->m_pHandle,
        UINT64_MAX,
        pSemaphore->m_pHandle,
        nullptr,
        &imageIdx);

    if (res != VK_SUCCESS)
        WaitForWork();

    return imageIdx;
}

void Device::Present(
    SwapChain *pSwapChain, u32 imageIdx, Semaphore *pSemaphore, CommandQueue *pQueue)
{
    ZoneScoped;

    VkPresentInfoKHR presentInfo = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &pSemaphore->m_pHandle,
        .swapchainCount = 1,
        .pSwapchains = &pSwapChain->m_pHandle,
        .pImageIndices = &imageIdx,
    };
    vkQueuePresentKHR(pQueue->m_pHandle, &presentInfo);
}

u64 Device::GetBufferMemorySize(Buffer *pBuffer, u64 *pAlignmentOut)
{
    ZoneScoped;

    VkMemoryRequirements memoryRequirements = {};
    vkGetBufferMemoryRequirements(m_pHandle, pBuffer->m_pHandle, &memoryRequirements);

    if (pAlignmentOut)
        *pAlignmentOut = memoryRequirements.alignment;

    return memoryRequirements.size;
}

u64 Device::GetImageMemorySize(Image *pImage, u64 *pAlignmentOut)
{
    ZoneScoped;

    VkMemoryRequirements memoryRequirements = {};
    vkGetImageMemoryRequirements(m_pHandle, pImage->m_pHandle, &memoryRequirements);

    if (pAlignmentOut)
        *pAlignmentOut = memoryRequirements.alignment;

    return memoryRequirements.size;
}

DeviceMemory *Device::CreateDeviceMemory(
    DeviceMemoryDesc *pDesc, PhysicalDevice *pPhysicalDevice)
{
    ZoneScoped;

    u32 heapIndex = pPhysicalDevice->GetHeapIndex((VkMemoryPropertyFlags)pDesc->m_Flags);
    DeviceMemory *pDeviceMemory = nullptr;
    switch (pDesc->m_Type)
    {
        case AllocatorType::Linear:
            pDeviceMemory = new LinearDeviceMemory;
            break;
        case AllocatorType::TLSF:
            pDeviceMemory = new TLSFDeviceMemory;
            break;
    }

    pDeviceMemory->m_pPhysicalDevice = pPhysicalDevice;
    VkMemoryAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = nullptr,
        .allocationSize = pDesc->m_Size,
        .memoryTypeIndex = heapIndex,
    };
    vkAllocateMemory(m_pHandle, &allocInfo, nullptr, &pDeviceMemory->m_pHandle);

    pDeviceMemory->Init(pDesc->m_Size);
    if (pDesc->m_Flags & MemoryFlag::HostVisible)
        vkMapMemory(
            m_pHandle,
            pDeviceMemory->m_pHandle,
            0,
            VK_WHOLE_SIZE,
            0,
            &pDeviceMemory->m_pMappedMemory);

    return pDeviceMemory;
}

void Device::DeleteDeviceMemory(DeviceMemory *pDeviceMemory)
{
    ZoneScoped;

    vkFreeMemory(m_pHandle, pDeviceMemory->m_pHandle, nullptr);
    delete pDeviceMemory;
}

void Device::AllocateBufferMemory(DeviceMemory *pMemory, Buffer *pBuffer, u64 memorySize)
{
    ZoneScoped;

    u64 alignment = 0;
    u64 alignedSize = GetBufferMemorySize(pBuffer, &alignment);
    u64 memoryOffset =
        pMemory->Allocate(alignedSize, alignment, pBuffer->m_AllocatorData);

    vkBindBufferMemory(m_pHandle, pBuffer->m_pHandle, pMemory->m_pHandle, memoryOffset);

    pBuffer->m_DataSize = alignedSize;
    pBuffer->m_DataOffset = memoryOffset;
}

void Device::AllocateImageMemory(DeviceMemory *pMemory, Image *pImage, u64 memorySize)
{
    ZoneScoped;

    u64 alignment = 0;
    u64 alignedSize = GetImageMemorySize(pImage, &alignment);
    u64 memoryOffset = pMemory->Allocate(alignedSize, alignment, pImage->m_AllocatorData);

    vkBindImageMemory(m_pHandle, pImage->m_pHandle, pMemory->m_pHandle, memoryOffset);

    pImage->m_DataSize = alignedSize;
    pImage->m_DataOffset = memoryOffset;
}

Shader *Device::CreateShader(ShaderStage stage, u32 *pData, u64 dataSize)
{
    ZoneScoped;

    Shader *pShader = new Shader;
    pShader->m_Type = stage;

    VkShaderModuleCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = dataSize,
        .pCode = pData,
    };
    vkCreateShaderModule(m_pHandle, &createInfo, nullptr, &pShader->m_pHandle);

    return pShader;
}

void Device::DeleteShader(Shader *pShader)
{
    ZoneScoped;

    vkDestroyShaderModule(m_pHandle, pShader->m_pHandle, nullptr);
    delete pShader;
}

DescriptorSetLayout Device::CreateDescriptorSetLayout(
    eastl::span<DescriptorLayoutElement> elements, DescriptorSetLayoutFlag flags)
{
    ZoneScoped;

    DescriptorSetLayout pLayout = nullptr;

    VkDescriptorSetLayoutCreateFlags createFlags = 0;
    if (flags & DescriptorSetLayoutFlag::DescriptorBuffer)
        createFlags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
    if (flags & DescriptorSetLayoutFlag::EmbeddedSamplers)
        createFlags |=
            VK_DESCRIPTOR_SET_LAYOUT_CREATE_EMBEDDED_IMMUTABLE_SAMPLERS_BIT_EXT;

    VkDescriptorSetLayoutCreateInfo layoutCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = createFlags,
        .bindingCount = (u32)elements.size(),
        .pBindings = elements.data(),
    };

    vkCreateDescriptorSetLayout(m_pHandle, &layoutCreateInfo, nullptr, &pLayout);

    return pLayout;
}

void Device::DeleteDescriptorSetLayout(DescriptorSetLayout pLayout)
{
    ZoneScoped;

    vkDestroyDescriptorSetLayout(m_pHandle, pLayout, nullptr);
}

u64 Device::GetDescriptorSetLayoutSize(DescriptorSetLayout pLayout)
{
    ZoneScoped;

    u64 size = 0;
    vkGetDescriptorSetLayoutSizeEXT(m_pHandle, pLayout, &size);

    return size;
}

u64 Device::GetDescriptorSetLayoutBindingOffset(
    DescriptorSetLayout pLayout, u32 bindingID)
{
    ZoneScoped;

    u64 offset = 0;
    vkGetDescriptorSetLayoutBindingOffsetEXT(m_pHandle, pLayout, bindingID, &offset);

    return offset;
}

void Device::GetDescriptorData(
    const DescriptorGetInfo &info, u64 dataSize, void *pDataOut)
{
    ZoneScoped;

    VkDescriptorAddressInfoEXT bufferInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT,
        .pNext = nullptr,
    };
    VkDescriptorImageInfo imageInfo = {};
    VkDescriptorDataEXT descriptorData = {};

    if (info.m_pBuffer)  // Allow null descriptors
    {
        switch (info.m_Type)
        {
            case DescriptorType::StorageBuffer:
            case DescriptorType::UniformBuffer:
                bufferInfo.address = info.m_pBuffer->m_DeviceAddress;
                bufferInfo.range = info.m_pBuffer->m_DataSize;
                descriptorData = { .pUniformBuffer = &bufferInfo };
                break;

            case DescriptorType::StorageImage:
            case DescriptorType::SampledImage:
                imageInfo.imageView = info.m_pImage->m_pViewHandle;
                descriptorData = { .pSampledImage = &imageInfo };
                break;

            case DescriptorType::Sampler:
                imageInfo.sampler = info.m_pSampler->m_pHandle;
                descriptorData = { .pSampledImage = &imageInfo };
                break;

            default:
                break;
        }
    }

    VkDescriptorGetInfoEXT vkInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT,
        .pNext = nullptr,
        .type = (VkDescriptorType)info.m_Type,
        .data = descriptorData,
    };

    vkGetDescriptorEXT(m_pHandle, &vkInfo, dataSize, pDataOut);
}

Buffer *Device::CreateBuffer(BufferDesc *pDesc, DeviceMemory *pAllocator)
{
    ZoneScoped;

    Buffer *pBuffer = new Buffer;
    u64 alignedSize =
        pAllocator->AlignBufferMemory(pDesc->m_UsageFlags, pDesc->m_DataSize);

    VkBufferCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,
        .size = alignedSize,
        .usage = (VkBufferUsageFlags)pDesc->m_UsageFlags
                 | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
    };
    vkCreateBuffer(m_pHandle, &createInfo, nullptr, &pBuffer->m_pHandle);
    AllocateBufferMemory(pAllocator, pBuffer, alignedSize);

    VkBufferDeviceAddressInfo deviceAddressInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .pNext = nullptr,
        .buffer = pBuffer->m_pHandle,
    };

    /// INIT BUFFER ///
    pBuffer->m_Stride = pDesc->m_Stride;
    pBuffer->m_DeviceAddress = vkGetBufferDeviceAddress(m_pHandle, &deviceAddressInfo);

    return pBuffer;
}

void Device::DeleteBuffer(Buffer *pBuffer, DeviceMemory *pAllocator)
{
    ZoneScoped;

    if (pAllocator)
        pAllocator->Free(pBuffer->m_AllocatorData);

    if (pBuffer->m_pHandle)
        vkDestroyBuffer(m_pHandle, pBuffer->m_pHandle, nullptr);
}

u8 *Device::GetMemoryData(DeviceMemory *pMemory)
{
    ZoneScoped;

    return (u8 *)pMemory->m_pMappedMemory;
}

u8 *Device::GetBufferMemoryData(DeviceMemory *pMemory, Buffer *pBuffer)
{
    ZoneScoped;

    return GetMemoryData(pMemory) + pBuffer->m_DataOffset;
}

Image *Device::CreateImage(ImageDesc *pDesc, DeviceMemory *pAllocator)
{
    ZoneScoped;

    Image *pImage = new Image;

    if (pDesc->m_DataSize == 0)
        pDesc->m_DataSize =
            pDesc->m_Width * pDesc->m_Height * FormatToSize(pDesc->m_Format);

    /// ---------------------------------------------- //

    VkImageCreateInfo imageCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = nullptr,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = (VkFormat)pDesc->m_Format,
        .extent.width = pDesc->m_Width,
        .extent.height = pDesc->m_Height,
        .extent.depth = 1,
        .mipLevels = pDesc->m_MipMapLevels,
        .arrayLayers = pDesc->m_ArraySize,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .usage = (VkImageUsageFlags)pDesc->m_UsageFlags,
        .sharingMode = pDesc->m_UsageFlags & ImageUsage::Concurrent
                           ? VK_SHARING_MODE_CONCURRENT
                           : VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = (u32)pDesc->m_QueueIndices.size(),
        .pQueueFamilyIndices = pDesc->m_QueueIndices.data(),
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    vkCreateImage(m_pHandle, &imageCreateInfo, nullptr, &pImage->m_pHandle);
    AllocateImageMemory(pAllocator, pImage, pDesc->m_DataSize);

    /// INIT BUFFER ///
    pImage->m_Format = pDesc->m_Format;
    pImage->m_Width = pDesc->m_Width;
    pImage->m_Height = pDesc->m_Height;
    pImage->m_ArraySize = pDesc->m_ArraySize;
    pImage->m_MipMapLevels = pDesc->m_MipMapLevels;

    CreateImageView(pImage, pDesc->m_UsageFlags);

    return pImage;
}

void Device::DeleteImage(Image *pImage, DeviceMemory *pAllocator)
{
    ZoneScoped;

    if (pAllocator)
        pAllocator->Free(pImage->m_AllocatorData);

    if (pImage->m_pViewHandle)
        vkDestroyImageView(m_pHandle, pImage->m_pViewHandle, nullptr);

    if (pImage->m_pHandle)
        vkDestroyImage(m_pHandle, pImage->m_pHandle, nullptr);
}

void Device::CreateImageView(Image *pImage, ImageUsage aspectUsage)
{
    ZoneScoped;

    VkImageAspectFlags aspectFlags = 0;
    if (aspectUsage & ImageUsage::AspectColor)
        aspectFlags |= VK_IMAGE_ASPECT_COLOR_BIT;
    if (aspectUsage & ImageUsage::AspectDepthStencil)
        aspectFlags |= VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;

    VkImageViewCreateInfo imageViewCreateInfo = {};
    imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;

    imageViewCreateInfo.image = pImage->m_pHandle;
    imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    imageViewCreateInfo.format = (VkFormat)pImage->m_Format;

    imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    imageViewCreateInfo.subresourceRange.aspectMask = aspectFlags;
    imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
    imageViewCreateInfo.subresourceRange.levelCount = pImage->m_MipMapLevels;
    imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
    imageViewCreateInfo.subresourceRange.layerCount = 1;

    vkCreateImageView(m_pHandle, &imageViewCreateInfo, nullptr, &pImage->m_pViewHandle);
}

Sampler *Device::CreateSampler(SamplerDesc *pDesc)
{
    ZoneScoped;

    Sampler *pSampler = new Sampler;

    VkSamplerCreateInfo samplerInfo = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .magFilter = (VkFilter)pDesc->m_MinFilter,
        .minFilter = (VkFilter)pDesc->m_MinFilter,
        .mipmapMode = (VkSamplerMipmapMode)pDesc->m_MipFilter,
        .addressModeU = (VkSamplerAddressMode)pDesc->m_AddressU,
        .addressModeV = (VkSamplerAddressMode)pDesc->m_AddressV,
        .addressModeW = (VkSamplerAddressMode)pDesc->m_AddressW,
        .mipLodBias = pDesc->m_MipLODBias,
        .anisotropyEnable = pDesc->m_UseAnisotropy,
        .maxAnisotropy = pDesc->m_MaxAnisotropy,
        .compareEnable = pDesc->m_CompareOp != CompareOp::Never,
        .compareOp = (VkCompareOp)pDesc->m_CompareOp,
        .minLod = pDesc->m_MinLOD,
        .maxLod = pDesc->m_MaxLOD,
        .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK,
    };
    vkCreateSampler(m_pHandle, &samplerInfo, nullptr, &pSampler->m_pHandle);

    return pSampler;
}

void Device::DeleteSampler(VkSampler pSampler)
{
    ZoneScoped;

    vkDestroySampler(m_pHandle, pSampler, nullptr);
}

}  // namespace lr::Graphics