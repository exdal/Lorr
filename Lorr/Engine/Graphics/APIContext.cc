// Created on Monday July 18th 2022 by exdal
// Last modified on Monday August 28th 2023 by exdal

#include "APIContext.hh"

#include "Core/Config.hh"
#include "Memory/MemoryUtils.hh"

#include "APIAllocator.hh"
#include "Shader.hh"

namespace lr::Graphics
{
bool APIContext::Init(APIContextDesc &desc, AllocatorDesc *pAllocatorOverrides)
{
    ZoneScoped;

    LOG_TRACE("Initializing Vulkan context...");

    constexpr u32 kTypeMem = Memory::MiBToBytes(16);
    APIAllocator::g_Handle.m_TypeAllocator.Init(kTypeMem, 0x2000);
    APIAllocator::g_Handle.m_pTypeData = Memory::Allocate<u8>(kTypeMem);

    AllocatorDesc allocatorDesc = {
        .m_MaxTLSFAllocations = CONFIG_GET_VAR(GPM_MAX_TLSF_ALLOCS),
        .m_DescriptorMem = CONFIG_GET_VAR(GPM_DESCRIPTOR),
        .m_BufferLinearMem = CONFIG_GET_VAR(GPM_BUFFER_LINEAR),
        .m_BufferTLSFMem = CONFIG_GET_VAR(GPM_BUFFER_TLSF),
        .m_BufferTLSFHostMem = CONFIG_GET_VAR(GPM_BUFFER_TLSF_HOST),
        .m_BufferFrametimeMem = CONFIG_GET_VAR(GPM_FRAMETIME),
        .m_ImageTLSFMem = CONFIG_GET_VAR(GPM_IMAGE_TLSF),
    };

    if (!pAllocatorOverrides)
        pAllocatorOverrides = &allocatorDesc;

    m_VulkanLib = VK::LoadVulkan();
    if (!m_VulkanLib)
        return false;

    if (!SetupInstance(desc.m_pTargetWindow))
        return false;

    if (!SetupPhysicalDevice())
        return false;

    if (!SetupSurface(desc.m_pTargetWindow))
        return false;

    m_pPhysicalDevice->InitQueueFamilies(m_pSurface);

    if (!SetupDevice())
        return false;

    m_pTracyCtx = TracyVkContextHostCalibrated(m_pPhysicalDevice->m_pHandle, m_pDevice);

    m_pSwapChain = CreateSwapChain(desc.m_pTargetWindow, desc.m_ImageCount, nullptr);
    SetObjectName(m_pPhysicalDevice, "Physical Device");
    SetObjectName(m_pSurface, "Default Surface");
    SetObjectName(m_pSwapChain, "Swap Chain");

    /// CREATE QUEUES ///

    m_pDirectQueue = CreateCommandQueue(CommandType::Graphics);
    SetObjectName(m_pDirectQueue, "Direct Queue");
    m_pComputeQueue = CreateCommandQueue(CommandType::Compute);
    SetObjectName(m_pComputeQueue, "Compute Queue");
    m_pTransferQueue = CreateCommandQueue(CommandType::Transfer);
    SetObjectName(m_pTransferQueue, "Transfer Queue");

    /// CREATE ALLOCATORS ///

    m_Allocators.m_pDescriptor = CreateLinearAllocator(
        MemoryFlag::HostVisibleCoherent, pAllocatorOverrides->m_DescriptorMem * m_pSwapChain->m_FrameCount);
    SetObjectName(m_Allocators.m_pDescriptor, "Descriptor");

    m_Allocators.m_pBufferLinear =
        CreateLinearAllocator(MemoryFlag::HostVisibleCoherent, pAllocatorOverrides->m_BufferLinearMem);
    SetObjectName(m_Allocators.m_pBufferLinear, "Buffer-Linear");

    m_Allocators.m_pBufferFrametime = CreateLinearAllocator(
        MemoryFlag::HostVisibleCoherent,
        pAllocatorOverrides->m_BufferFrametimeMem * m_pSwapChain->m_FrameCount);
    SetObjectName(m_Allocators.m_pBufferFrametime, "Buffer-Frametime");

    m_Allocators.m_pBufferTLSF = CreateTLSFAllocator(
        MemoryFlag::Device, pAllocatorOverrides->m_BufferTLSFMem, pAllocatorOverrides->m_MaxTLSFAllocations);
    SetObjectName(m_Allocators.m_pBufferTLSF, "Buffer-TLSF");

    m_Allocators.m_pBufferTLSFHost = CreateTLSFAllocator(
        MemoryFlag::HostVisibleCoherent,
        pAllocatorOverrides->m_BufferTLSFHostMem,
        pAllocatorOverrides->m_MaxTLSFAllocations);
    SetObjectName(m_Allocators.m_pBufferTLSFHost, "Buffer-TLSF-Host");

    m_Allocators.m_pImageTLSF = CreateTLSFAllocator(
        MemoryFlag::Device, pAllocatorOverrides->m_ImageTLSFMem, pAllocatorOverrides->m_MaxTLSFAllocations);
    SetObjectName(m_Allocators.m_pImageTLSF, "Image-TLSF");

    m_pPipelineCache = CreatePipelineCache();

    LOG_TRACE("Successfully initialized Vulkan API.");

    return true;
}

u32 APIContext::GetQueueIndex(CommandType type)
{
    ZoneScoped;

    return m_pPhysicalDevice->GetQueueIndex(type);
}

CommandQueue *APIContext::CreateCommandQueue(CommandType type)
{
    ZoneScoped;

    CommandQueue *pQueue = new CommandQueue;
    vkGetDeviceQueue(m_pDevice, m_pPhysicalDevice->GetQueueIndex(type), 0, &pQueue->m_pHandle);

    return pQueue;
}

CommandAllocator *APIContext::CreateCommandAllocator(CommandType type, bool resetLists)
{
    ZoneScoped;

    CommandAllocator *pAllocator = new CommandAllocator;

    VkCommandPoolCreateInfo allocatorInfo = {};
    allocatorInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    allocatorInfo.flags = resetLists ? VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT : 0;
    allocatorInfo.queueFamilyIndex = m_pPhysicalDevice->GetQueueIndex(type);

    vkCreateCommandPool(m_pDevice, &allocatorInfo, nullptr, &pAllocator->m_pHandle);

    return pAllocator;
}

CommandList *APIContext::CreateCommandList(CommandType type, CommandAllocator *pAllocator)
{
    ZoneScoped;

    CommandList *pList = new CommandList;
    pList->m_Type = type;

    if (pAllocator)
        AllocateCommandList(pList, pAllocator);

    return pList;
}

void APIContext::AllocateCommandList(CommandList *pList, CommandAllocator *pAllocator)
{
    ZoneScoped;

    pList->m_pAllocator = pAllocator;

    VkCommandBufferAllocateInfo listInfo = {};
    listInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    listInfo.commandBufferCount = 1;
    listInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    listInfo.commandPool = pAllocator->m_pHandle;
    vkAllocateCommandBuffers(m_pDevice, &listInfo, &pList->m_pHandle);
}

void APIContext::BeginCommandList(CommandList *pList)
{
    ZoneScoped;

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.pNext = nullptr;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(pList->m_pHandle, &beginInfo);
}

void APIContext::EndCommandList(CommandList *pList)
{
    ZoneScoped;

    TracyVkCollect(m_pTracyCtx, pList->m_pHandle);
    vkEndCommandBuffer(pList->m_pHandle);
}

void APIContext::ResetCommandAllocator(CommandAllocator *pAllocator)
{
    ZoneScoped;

    vkResetCommandPool(m_pDevice, pAllocator->m_pHandle, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT);
}

void APIContext::Submit(SubmitDesc *pDesc)
{
    ZoneScoped;

    VkQueue pQueueHandle = nullptr;
    switch (pDesc->m_Type)
    {
        case CommandType::Graphics:
            pQueueHandle = m_pDirectQueue->m_pHandle;
            break;
        case CommandType::Compute:
            pQueueHandle = m_pComputeQueue->m_pHandle;
            break;
        case CommandType::Transfer:
            pQueueHandle = m_pTransferQueue->m_pHandle;
            break;
        default:
            break;
    }

    VkSubmitInfo2 submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submitInfo.waitSemaphoreInfoCount = pDesc->m_WaitSemas.size();
    submitInfo.pWaitSemaphoreInfos = pDesc->m_WaitSemas.data();
    submitInfo.commandBufferInfoCount = pDesc->m_Lists.size();
    submitInfo.pCommandBufferInfos = pDesc->m_Lists.data();
    submitInfo.signalSemaphoreInfoCount = pDesc->m_SignalSemas.size();
    submitInfo.pSignalSemaphoreInfos = pDesc->m_SignalSemas.begin();

    vkQueueSubmit2(pQueueHandle, 1, &submitInfo, nullptr);
}

Fence *APIContext::CreateFence(bool signaled)
{
    ZoneScoped;

    Fence *pFence = new Fence;

    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = signaled;

    vkCreateFence(m_pDevice, &fenceInfo, nullptr, &pFence->m_pHandle);

    return pFence;
}

void APIContext::DeleteFence(Fence *pFence)
{
    ZoneScoped;

    vkDestroyFence(m_pDevice, pFence->m_pHandle, nullptr);

    delete pFence;
}

bool APIContext::IsFenceSignaled(Fence *pFence)
{
    ZoneScoped;

    return vkGetFenceStatus(m_pDevice, pFence->m_pHandle) == VK_SUCCESS;
}

void APIContext::ResetFence(Fence *pFence)
{
    ZoneScoped;

    vkResetFences(m_pDevice, 1, &pFence->m_pHandle);
}

Semaphore *APIContext::CreateSemaphore(u32 initialValue, bool binary)
{
    ZoneScoped;

    Semaphore *pSemaphore = new Semaphore;
    pSemaphore->m_Value = initialValue;

    VkSemaphoreTypeCreateInfo semaphoreTypeInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
        .semaphoreType = binary ? VK_SEMAPHORE_TYPE_BINARY : VK_SEMAPHORE_TYPE_TIMELINE,
        .initialValue = pSemaphore->m_Value,
    };

    VkSemaphoreCreateInfo semaphoreInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = &semaphoreTypeInfo,
    };

    vkCreateSemaphore(m_pDevice, &semaphoreInfo, nullptr, &pSemaphore->m_pHandle);

    return pSemaphore;
}

void APIContext::DeleteSemaphore(Semaphore *pSemaphore)
{
    ZoneScoped;

    vkDestroySemaphore(m_pDevice, pSemaphore->m_pHandle, nullptr);
    delete pSemaphore;
}

void APIContext::WaitForSemaphore(Semaphore *pSemaphore, u64 desiredValue, u64 timeout)
{
    ZoneScoped;

    VkSemaphoreWaitInfo waitInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
        .semaphoreCount = 1,
        .pSemaphores = &pSemaphore->m_pHandle,
        .pValues = &desiredValue,
    };

    vkWaitSemaphores(m_pDevice, &waitInfo, timeout);
}

VkPipelineCache APIContext::CreatePipelineCache(u32 initialDataSize, void *pInitialData)
{
    ZoneScoped;

    VkPipelineCacheCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
        .initialDataSize = initialDataSize,
        .pInitialData = pInitialData,
    };

    VkPipelineCache pHandle = nullptr;
    vkCreatePipelineCache(m_pDevice, &createInfo, nullptr, &pHandle);

    return pHandle;
}

PipelineLayout *APIContext::CreatePipelineLayout(
    eastl::span<DescriptorSetLayout *> layouts, eastl::span<PushConstantDesc> pushConstants)
{
    ZoneScoped;

    eastl::vector<VkDescriptorSetLayout> layoutHandles(layouts.size());

    u32 idx = 0;
    for (DescriptorSetLayout *pLayout : layouts)
        layoutHandles[idx++] = pLayout->m_pHandle;

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .setLayoutCount = (u32)layouts.size(),
        .pSetLayouts = layoutHandles.data(),
        .pushConstantRangeCount = (u32)pushConstants.size(),
        .pPushConstantRanges = pushConstants.data(),
    };

    PipelineLayout *pLayout = new PipelineLayout;
    vkCreatePipelineLayout(m_pDevice, &pipelineLayoutCreateInfo, nullptr, &pLayout->m_pHandle);

    return pLayout;
}

Pipeline *APIContext::CreateGraphicsPipeline(GraphicsPipelineBuildInfo *pBuildInfo)
{
    ZoneScoped;

    Pipeline *pPipeline = new Pipeline;
    pPipeline->m_BindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    pPipeline->m_pLayout = pBuildInfo->m_pLayout;

    /// BOUND RENDER TARGETS -----------------------------------------------------

    VkFormat pRenderTargetFormats[Limits::MaxColorAttachments] = {};

    VkPipelineRenderingCreateInfo renderingInfo = {};
    renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    renderingInfo.pNext = nullptr;
    renderingInfo.viewMask = 0;
    renderingInfo.colorAttachmentCount = pBuildInfo->m_ColorAttachmentFormats.size();
    renderingInfo.pColorAttachmentFormats = pRenderTargetFormats;
    renderingInfo.depthAttachmentFormat = (VkFormat)pBuildInfo->m_DepthAttachmentFormat;
    for (u32 i = 0; i < pBuildInfo->m_ColorAttachmentFormats.size(); i++)
        pRenderTargetFormats[i] = (VkFormat)pBuildInfo->m_ColorAttachmentFormats[i];

    /// BOUND SHADER STAGES ------------------------------------------------------

    VkPipelineShaderStageCreateInfo pShaderStageInfos[(u32)ShaderStage::Count] = {};
    for (u32 i = 0; i < pBuildInfo->m_Shaders.size(); i++)
    {
        Shader *pShader = pBuildInfo->m_Shaders[i];

        VkPipelineShaderStageCreateInfo &info = pShaderStageInfos[i];
        info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        info.pName = nullptr;
        info.stage = (VkShaderStageFlagBits)pShader->m_Type;
        info.module = pShader->m_pHandle;
        info.pName = "main";
    }

    /// INPUT LAYOUT  ------------------------------------------------------------

    VkPipelineVertexInputStateCreateInfo inputLayoutInfo = {};
    inputLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    inputLayoutInfo.pNext = nullptr;

    VkVertexInputBindingDescription inputBindingInfo = {};
    VkVertexInputAttributeDescription pAttribInfos[Limits::MaxVertexAttribs] = {};

    /// INPUT ASSEMBLY -----------------------------------------------------------

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo = {};
    inputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssemblyInfo.pNext = nullptr;
    inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    /// TESSELLATION -------------------------------------------------------------

    VkPipelineTessellationStateCreateInfo tessellationInfo = {};
    tessellationInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
    tessellationInfo.pNext = nullptr;
    tessellationInfo.patchControlPoints = 0;  // TODO

    /// VIEWPORT -----------------------------------------------------------------

    VkPipelineViewportStateCreateInfo viewportInfo = {};
    viewportInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportInfo.pNext = nullptr;
    viewportInfo.viewportCount = 1;
    viewportInfo.pViewports = nullptr;
    viewportInfo.scissorCount = 1;
    viewportInfo.pScissors = nullptr;

    /// RASTERIZER ---------------------------------------------------------------

    VkPipelineRasterizationStateCreateInfo rasterizerInfo = {};
    rasterizerInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizerInfo.pNext = nullptr;
    rasterizerInfo.depthClampEnable = pBuildInfo->m_EnableDepthClamp;
    rasterizerInfo.polygonMode = (VkPolygonMode)pBuildInfo->m_SetFillMode;
    rasterizerInfo.cullMode = (VkCullModeFlags)pBuildInfo->m_SetCullMode;
    rasterizerInfo.frontFace = (VkFrontFace)!pBuildInfo->m_FrontFaceCCW;
    rasterizerInfo.depthBiasEnable = pBuildInfo->m_EnableDepthBias;
    rasterizerInfo.depthBiasConstantFactor = pBuildInfo->m_DepthBiasFactor;
    rasterizerInfo.depthBiasClamp = pBuildInfo->m_DepthBiasClamp;
    rasterizerInfo.depthBiasSlopeFactor = pBuildInfo->m_DepthSlopeFactor;
    rasterizerInfo.lineWidth = 1.0;

    /// MULTISAMPLE --------------------------------------------------------------

    VkPipelineMultisampleStateCreateInfo multisampleInfo = {};
    multisampleInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampleInfo.pNext = nullptr;
    multisampleInfo.rasterizationSamples =
        (VkSampleCountFlagBits)(1 << (pBuildInfo->m_MultiSampleBitCount - 1));
    multisampleInfo.alphaToCoverageEnable = pBuildInfo->m_EnableAlphaToCoverage;
    multisampleInfo.alphaToOneEnable = false;

    /// DEPTH STENCIL ------------------------------------------------------------

    VkPipelineDepthStencilStateCreateInfo depthStencilInfo = {};
    depthStencilInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencilInfo.pNext = nullptr;
    depthStencilInfo.depthTestEnable = pBuildInfo->m_EnableDepthTest;
    depthStencilInfo.depthWriteEnable = pBuildInfo->m_EnableDepthWrite;
    depthStencilInfo.depthCompareOp = (VkCompareOp)pBuildInfo->m_DepthCompareOp;
    depthStencilInfo.depthBoundsTestEnable = false;
    depthStencilInfo.stencilTestEnable = pBuildInfo->m_EnableStencilTest;

    depthStencilInfo.front.compareOp = (VkCompareOp)pBuildInfo->m_StencilFrontFaceOp.m_CompareFunc;
    depthStencilInfo.front.depthFailOp = (VkStencilOp)pBuildInfo->m_StencilFrontFaceOp.m_DepthFail;
    depthStencilInfo.front.failOp = (VkStencilOp)pBuildInfo->m_StencilFrontFaceOp.m_Fail;
    depthStencilInfo.front.passOp = (VkStencilOp)pBuildInfo->m_StencilFrontFaceOp.m_Pass;

    depthStencilInfo.back.compareOp = (VkCompareOp)pBuildInfo->m_StencilBackFaceOp.m_CompareFunc;
    depthStencilInfo.back.depthFailOp = (VkStencilOp)pBuildInfo->m_StencilBackFaceOp.m_DepthFail;
    depthStencilInfo.back.failOp = (VkStencilOp)pBuildInfo->m_StencilBackFaceOp.m_Fail;
    depthStencilInfo.back.passOp = (VkStencilOp)pBuildInfo->m_StencilBackFaceOp.m_Pass;

    /// COLOR BLEND --------------------------------------------------------------

    VkPipelineColorBlendStateCreateInfo colorBlendInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .pNext = nullptr,
        .logicOpEnable = false,
        .attachmentCount = (u32)pBuildInfo->m_BlendAttachments.size(),
        .pAttachments = pBuildInfo->m_BlendAttachments.data(),
    };

    /// DYNAMIC STATE ------------------------------------------------------------

    constexpr static eastl::array<VkDynamicState, 2> kDynamicStates = { VK_DYNAMIC_STATE_VIEWPORT,
                                                                        VK_DYNAMIC_STATE_SCISSOR };

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
        .layout = pBuildInfo->m_pLayout->m_pHandle,
    };

    vkCreateGraphicsPipelines(m_pDevice, m_pPipelineCache, 1, &createInfo, nullptr, &pPipeline->m_pHandle);

    return pPipeline;
}

Pipeline *APIContext::CreateComputePipeline(ComputePipelineBuildInfo *pBuildInfo)
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
        .layout = pBuildInfo->m_pLayout->m_pHandle,
    };

    vkCreateComputePipelines(m_pDevice, m_pPipelineCache, 1, &createInfo, nullptr, &pPipeline->m_pHandle);

    return pPipeline;
}

SwapChain *APIContext::CreateSwapChain(BaseWindow *pWindow, u32 imageCount, SwapChain *pOldSwapChain)
{
    ZoneScoped;

    SwapChain *pSwapChain = pOldSwapChain;

    if (!pOldSwapChain)
    {
        pSwapChain = new SwapChain;

        VkFormat format = (VkFormat)pSwapChain->m_ImageFormat;

        if (!m_pSurface->IsPresentModeSupported(pSwapChain->m_PresentMode))
            imageCount = 1;

        if (imageCount == 1)
        {
            pSwapChain->m_PresentMode = VK_PRESENT_MODE_FIFO_KHR;
            pSwapChain->m_FrameCount = 1;
        }
        else
        {
            pSwapChain->m_FrameCount = imageCount;
        }

        pSwapChain->m_FrameCount =
            eastl::min(pSwapChain->m_FrameCount, m_pSurface->m_MaxImageCount);  // just to make sure

        if (!m_pSurface->IsFormatSupported(format, pSwapChain->m_ColorSpace))
        {
            LOG_CRITICAL("SwapChain does not support format {}!", (u32)format);
            return nullptr;
        }
    }

    pSwapChain->m_Width = pWindow->m_Width;
    pSwapChain->m_Height = pWindow->m_Height;

    VkSwapchainCreateInfoKHR swapChainInfo = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .pNext = nullptr,
        .surface = m_pSurface->m_pHandle,
        .minImageCount = pSwapChain->m_FrameCount,
        .imageFormat = (VkFormat)pSwapChain->m_ImageFormat,
        .imageColorSpace = pSwapChain->m_ColorSpace,
        .imageExtent = { pSwapChain->m_Width, pSwapChain->m_Height },
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,
        .preTransform = m_pSurface->m_CurrentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = pSwapChain->m_PresentMode,
        .clipped = VK_TRUE,
    };

    VkResult result = vkCreateSwapchainKHR(m_pDevice, &swapChainInfo, nullptr, &pSwapChain->m_pHandle);

    VkImage ppSwapChainImages[Limits::MaxFrameCount] = {};
    vkGetSwapchainImagesKHR(m_pDevice, pSwapChain->m_pHandle, &pSwapChain->m_FrameCount, &ppSwapChainImages[0]);

    for (u32 i = 0; i < pSwapChain->m_FrameCount; i++)
    {
        SwapChainFrame &frame = pSwapChain->m_Frames[i];
        Image *&pImage = frame.m_pImage;

        pImage = new Image;
        pImage->m_pHandle = ppSwapChainImages[i];
        pImage->m_Width = pSwapChain->m_Width;
        pImage->m_Height = pSwapChain->m_Height;
        pImage->m_DataSize = ~0;
        pImage->m_DataOffset = ~0;
        pImage->m_MipMapLevels = 1;
        pImage->m_Format = pSwapChain->m_ImageFormat;
        pImage->m_Allocator = ResourceAllocator::Count;
        CreateImageView(pImage, ImageUsage::ColorAttachment);

        SetObjectName(pImage, _FMT("Swap Chain Image {}", i));

        frame.m_pPresentSemp = CreateSemaphore();
        SetObjectName(frame.m_pPresentSemp, _FMT("Present Semaphore {}", i));
    }

    return pSwapChain;
}

void APIContext::DeleteSwapChain(SwapChain *pSwapChain, bool keepSelf)
{
    ZoneScoped;

    for (u32 i = 0; i < pSwapChain->m_FrameCount; i++)
    {
        SwapChainFrame &frame = pSwapChain->m_Frames[i];
        DeleteSemaphore(frame.m_pPresentSemp);

        auto &bufferQueue = frame.m_BufferDeleteQueue;
        for (u32 i = 0; i < bufferQueue.size(); i++)
        {
            Buffer *pBuffer = bufferQueue.front();
            DeleteBuffer(pBuffer, false);
            bufferQueue.pop();
        }

        auto &imageQueue = frame.m_ImageDeleteQueue;
        for (u32 i = 0; i < imageQueue.size(); i++)
        {
            Image *pImage = imageQueue.front();
            DeleteImage(pImage, false);
            imageQueue.pop();
        }

        vkDestroyImageView(m_pDevice, frame.m_pImage->m_pViewHandle, nullptr);
    }

    for (Semaphore *pSemaphore : m_AcquireSempPool)
        DeleteSemaphore(pSemaphore);

    m_AcquireSempPool.clear();

    vkDestroySwapchainKHR(m_pDevice, pSwapChain->m_pHandle, nullptr);

    if (!keepSelf)
        delete pSwapChain;
}

void APIContext::ResizeSwapChain(BaseWindow *pWindow)
{
    ZoneScoped;

    WaitForWork();

    DeleteSwapChain(m_pSwapChain, true);
    m_pSwapChain = CreateSwapChain(pWindow, m_pSwapChain->m_FrameCount, m_pSwapChain);
}

SwapChain *APIContext::GetSwapChain()
{
    return m_pSwapChain;
}

void APIContext::WaitForWork()
{
    ZoneScoped;

    vkDeviceWaitIdle(m_pDevice);
}

void APIContext::BeginFrame()
{
    ZoneScoped;

    SwapChainFrame *pCurrentFrame = m_pSwapChain->GetCurrentFrame();
    Semaphore *pSemaphore = GetAvailableAcquireSemaphore(pCurrentFrame->m_pAcquireSemp);

    u32 imageIdx = 0;
    VkResult res = vkAcquireNextImageKHR(
        m_pDevice, m_pSwapChain->m_pHandle, UINT64_MAX, pSemaphore->m_pHandle, nullptr, &imageIdx);
    if (res != VK_SUCCESS)
    {
        WaitForWork();
    }

    m_pSwapChain->Advance(imageIdx, pSemaphore);

    /// PAST FRAME WORK ///
    pCurrentFrame = m_pSwapChain->GetCurrentFrame();

    auto &bufferQueue = pCurrentFrame->m_BufferDeleteQueue;
    for (u32 i = 0; i < bufferQueue.size(); i++)
    {
        Buffer *pBuffer = bufferQueue.front();
        DeleteBuffer(pBuffer, false);
        bufferQueue.pop();
    }

    auto &imageQueue = pCurrentFrame->m_ImageDeleteQueue;
    for (u32 i = 0; i < imageQueue.size(); i++)
    {
        Image *pImage = imageQueue.front();
        DeleteImage(pImage, false);
        imageQueue.pop();
    }

    m_Allocators.m_pBufferFrametime->Free(0);
}

void APIContext::EndFrame()
{
    ZoneScoped;

    SwapChainFrame *pCurrentFrame = m_pSwapChain->GetCurrentFrame();

    VkQueue pQueue = m_pDirectQueue->m_pHandle;
    VkSemaphore acqireSem = pCurrentFrame->m_pAcquireSemp->m_pHandle;
    VkSemaphore presentSem = pCurrentFrame->m_pPresentSemp->m_pHandle;

    VkSemaphoreSubmitInfo acquireSemaphoreInfo = {};
    acquireSemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    acquireSemaphoreInfo.semaphore = acqireSem;
    acquireSemaphoreInfo.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSemaphoreSubmitInfo presentSemaphoreInfo = {};
    presentSemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    presentSemaphoreInfo.semaphore = presentSem;
    presentSemaphoreInfo.stageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;

    VkSubmitInfo2 submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submitInfo.waitSemaphoreInfoCount = 1;
    submitInfo.pWaitSemaphoreInfos = &acquireSemaphoreInfo;
    submitInfo.signalSemaphoreInfoCount = 1;
    submitInfo.pSignalSemaphoreInfos = &presentSemaphoreInfo;
    vkQueueSubmit2(pQueue, 1, &submitInfo, nullptr);

    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &presentSem;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &m_pSwapChain->m_pHandle;
    presentInfo.pImageIndices = &m_pSwapChain->m_CurrentFrame;
    vkQueuePresentKHR(pQueue, &presentInfo);
}

u64 APIContext::GetBufferMemorySize(Buffer *pBuffer, u64 *pAlignmentOut)
{
    ZoneScoped;

    VkMemoryRequirements memoryRequirements = {};
    vkGetBufferMemoryRequirements(m_pDevice, pBuffer->m_pHandle, &memoryRequirements);

    if (pAlignmentOut)
        *pAlignmentOut = memoryRequirements.alignment;

    return memoryRequirements.size;
}

u64 APIContext::GetImageMemorySize(Image *pImage, u64 *pAlignmentOut)
{
    ZoneScoped;

    VkMemoryRequirements memoryRequirements = {};
    vkGetImageMemoryRequirements(m_pDevice, pImage->m_pHandle, &memoryRequirements);

    if (pAlignmentOut)
        *pAlignmentOut = memoryRequirements.alignment;

    return memoryRequirements.size;
}

VkDeviceMemory APIContext::CreateDeviceMemory(MemoryFlag memoryFlags, u64 memorySize)
{
    ZoneScoped;

    VkMemoryPropertyFlags memoryProps = 0;
    if (memoryFlags & MemoryFlag::Device)
        memoryProps |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    if (memoryFlags & MemoryFlag::HostVisible)
        memoryProps |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    if (memoryFlags & MemoryFlag::HostCoherent)
        memoryProps |= VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    if (memoryFlags & MemoryFlag::HostCached)
        memoryProps |= VK_MEMORY_PROPERTY_HOST_CACHED_BIT;

    VkDeviceMemory pDeviceMem = VK_NULL_HANDLE;
    VkMemoryAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = nullptr,
        .allocationSize = memorySize,
        .memoryTypeIndex = m_pPhysicalDevice->GetHeapIndex(memoryProps),
    };
    vkAllocateMemory(m_pDevice, &allocInfo, nullptr, &pDeviceMem);

    return pDeviceMem;
}

LinearDeviceMemory *APIContext::CreateLinearAllocator(MemoryFlag memoryFlags, u64 memorySize)
{
    ZoneScoped;

    LinearDeviceMemory *pDeviceMem = new LinearDeviceMemory;
    pDeviceMem->Init(memorySize);
    pDeviceMem->m_pHandle = CreateDeviceMemory(memoryFlags, memorySize);

    if (memoryFlags & MemoryFlag::HostVisible)
        vkMapMemory(m_pDevice, pDeviceMem->m_pHandle, 0, VK_WHOLE_SIZE, 0, &pDeviceMem->m_pMappedMemory);

    return pDeviceMem;
}

TLSFDeviceMemory *APIContext::CreateTLSFAllocator(MemoryFlag memoryFlags, u64 memorySize, u32 maxAllocs)
{
    ZoneScoped;

    TLSFDeviceMemory *pDeviceMem = new TLSFDeviceMemory;
    pDeviceMem->m_Allocator.Init(memorySize, maxAllocs);
    pDeviceMem->m_pHandle = CreateDeviceMemory(memoryFlags, memorySize);

    if (memoryFlags & MemoryFlag::HostVisible)
        vkMapMemory(m_pDevice, pDeviceMem->m_pHandle, 0, VK_WHOLE_SIZE, 0, &pDeviceMem->m_pMappedMemory);

    return pDeviceMem;
}

void APIContext::DeleteAllocator(DeviceMemory *pDeviceMemory)
{
    ZoneScoped;

    vkFreeMemory(m_pDevice, pDeviceMemory->m_pHandle, nullptr);
}

u64 APIContext::OffsetFrametimeMemory(u64 size, u64 offset)
{
    ZoneScoped;

    return offset + (m_pSwapChain->m_CurrentFrame * size);
}

void APIContext::AllocateBufferMemory(ResourceAllocator allocator, Buffer *pBuffer, u64 memorySize)
{
    ZoneScoped;
    assert(allocator != ResourceAllocator::None);

    u64 alignment = 0;
    u64 alignedSize = GetBufferMemorySize(pBuffer, &alignment);
    DeviceMemory *pMemory = m_Allocators.GetDeviceMemory(allocator);
    u64 memoryOffset = pMemory->Allocate(alignedSize, alignment, pBuffer->m_AllocatorData);

    vkBindBufferMemory(m_pDevice, pBuffer->m_pHandle, pMemory->m_pHandle, memoryOffset);

    pBuffer->m_DataSize = alignedSize;
    pBuffer->m_DataOffset = memoryOffset;
}

void APIContext::AllocateImageMemory(ResourceAllocator allocator, Image *pImage, u64 memorySize)
{
    ZoneScoped;

    assert(allocator != ResourceAllocator::None);

    u64 alignment = 0;
    u64 alignedSize = GetImageMemorySize(pImage, &alignment);
    DeviceMemory *pMemory = m_Allocators.GetDeviceMemory(allocator);
    u64 memoryOffset = pMemory->Allocate(alignedSize, alignment, pImage->m_AllocatorData);

    vkBindImageMemory(m_pDevice, pImage->m_pHandle, pMemory->m_pHandle, memoryOffset);

    pImage->m_DataSize = alignedSize;
    pImage->m_DataOffset = memoryOffset;
}

Shader *APIContext::CreateShader(ShaderStage stage, u32 *pData, u64 dataSize)
{
    ZoneScoped;

    Shader *pShader = new Shader;
    pShader->m_Type = stage;

    VkShaderModuleCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = dataSize,
        .pCode = pData,
    };
    vkCreateShaderModule(m_pDevice, &createInfo, nullptr, &pShader->m_pHandle);

    return pShader;
}

void APIContext::DeleteShader(Shader *pShader)
{
    ZoneScoped;

    vkDestroyShaderModule(m_pDevice, pShader->m_pHandle, nullptr);
    delete pShader;
}

DescriptorSetLayout *APIContext::CreateDescriptorSetLayout(
    eastl::span<DescriptorLayoutElement> elements, DescriptorSetLayoutFlag flags)
{
    ZoneScoped;

    DescriptorSetLayout *pLayout = new DescriptorSetLayout;

    VkDescriptorSetLayoutCreateFlags createFlags = 0;
    if (flags & DescriptorSetLayoutFlag::DescriptorBuffer)
        createFlags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
    if (flags & DescriptorSetLayoutFlag::EmbeddedSamplers)
        createFlags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_EMBEDDED_IMMUTABLE_SAMPLERS_BIT_EXT;

    VkDescriptorSetLayoutCreateInfo layoutCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = createFlags,
        .bindingCount = (u32)elements.size(),
        .pBindings = elements.data(),
    };

    vkCreateDescriptorSetLayout(m_pDevice, &layoutCreateInfo, nullptr, &pLayout->m_pHandle);

    return pLayout;
}

void APIContext::DeleteDescriptorSetLayout(DescriptorSetLayout *pLayout)
{
    ZoneScoped;

    vkDestroyDescriptorSetLayout(m_pDevice, pLayout->m_pHandle, nullptr);

    delete pLayout;
}

u64 APIContext::GetDescriptorSetLayoutSize(DescriptorSetLayout *pLayout)
{
    ZoneScoped;

    u64 size = 0;
    vkGetDescriptorSetLayoutSizeEXT(m_pDevice, pLayout->m_pHandle, &size);

    return Memory::AlignUp(size, m_pPhysicalDevice->GetDescriptorBufferAlignment());
}

u64 APIContext::GetDescriptorSetLayoutBindingOffset(DescriptorSetLayout *pLayout, u32 bindingID)
{
    ZoneScoped;

    u64 offset = 0;
    vkGetDescriptorSetLayoutBindingOffsetEXT(m_pDevice, pLayout->m_pHandle, bindingID, &offset);

    return offset;
}

u64 APIContext::GetDescriptorSize(DescriptorType type)
{
    ZoneScoped;

    VkPhysicalDeviceDescriptorBufferPropertiesEXT &props = m_pPhysicalDevice->m_FeatureDescriptorBufferProps;
    u64 sizeArray[] = {
        props.samplerDescriptorSize,        // Sampler,
        props.sampledImageDescriptorSize,   // SampledImage,
        props.uniformBufferDescriptorSize,  // UniformBuffer,
        props.storageImageDescriptorSize,   // StorageImage,
        props.storageBufferDescriptorSize,  // StorageBuffer,
    };

    return sizeArray[(u32)type];
}

u64 APIContext::GetDescriptorSizeAligned(DescriptorType type)
{
    ZoneScoped;

    return AlignUpDescriptorOffset(GetDescriptorSize(type));
}

u64 APIContext::AlignUpDescriptorOffset(u64 offset)
{
    ZoneScoped;

    VkPhysicalDeviceDescriptorBufferPropertiesEXT &props = m_pPhysicalDevice->m_FeatureDescriptorBufferProps;
    return Memory::AlignUp(offset, props.descriptorBufferOffsetAlignment);
}

void APIContext::GetDescriptorData(
    const DescriptorGetInfo &info, u64 dataSize, void *pDataOut, u32 descriptorIdx)
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
                info.m_pBuffer->m_DescriptorIndex = descriptorIdx;
                bufferInfo.address = info.m_pBuffer->m_DeviceAddress;
                bufferInfo.range = info.m_pBuffer->m_DataSize;
                descriptorData = { .pUniformBuffer = &bufferInfo };
                break;

            case DescriptorType::StorageImage:
            case DescriptorType::SampledImage:
                info.m_pImage->m_DescriptorIndex = descriptorIdx;
                imageInfo.imageView = info.m_pImage->m_pViewHandle;
                descriptorData = { .pSampledImage = &imageInfo };
                break;

            case DescriptorType::Sampler:
                info.m_pSampler->m_DescriptorIndex = descriptorIdx;
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

    vkGetDescriptorEXT(m_pDevice, &vkInfo, dataSize, pDataOut);
}

Buffer *APIContext::CreateBuffer(BufferDesc *pDesc)
{
    ZoneScoped;

    Buffer *pBuffer = new Buffer;

    /// Some buffer types, such as descriptor buffers, require explicit alignment
    u64 alignedBufferSize = pDesc->m_DataSize;
    if (pDesc->m_UsageFlags & (BufferUsage::ResourceDescriptor | BufferUsage::SamplerDescriptor))
        alignedBufferSize =
            Memory::AlignUp(pDesc->m_DataSize, m_pPhysicalDevice->GetDescriptorBufferAlignment());

    VkBufferCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,
        .size = alignedBufferSize,
        .usage = (VkBufferUsageFlags)pDesc->m_UsageFlags | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
    };
    vkCreateBuffer(m_pDevice, &createInfo, nullptr, &pBuffer->m_pHandle);
    AllocateBufferMemory(pDesc->m_TargetAllocator, pBuffer, pDesc->m_DataSize);

    VkBufferDeviceAddressInfo deviceAddressInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .pNext = nullptr,
        .buffer = pBuffer->m_pHandle,
    };

    /// INIT BUFFER ///
    pBuffer->m_Allocator = pDesc->m_TargetAllocator;
    pBuffer->m_Stride = pDesc->m_Stride;
    pBuffer->m_DeviceAddress = vkGetBufferDeviceAddress(m_pDevice, &deviceAddressInfo);

    return pBuffer;
}

void APIContext::DeleteBuffer(Buffer *pBuffer, bool waitForWork)
{
    ZoneScoped;

    assert(pBuffer->m_Allocator != ResourceAllocator::None);

    if (waitForWork)
    {
        SwapChainFrame *pCurrentFrame = m_pSwapChain->GetCurrentFrame();
        pCurrentFrame->m_BufferDeleteQueue.push(pBuffer);
        return;
    }

    DeviceMemory *pMemory = m_Allocators.GetDeviceMemory(pBuffer->m_Allocator);
    if (pMemory)
        pMemory->Free(pBuffer->m_AllocatorData);

    if (pBuffer->m_pHandle)
        vkDestroyBuffer(m_pDevice, pBuffer->m_pHandle, nullptr);
}

u8 *APIContext::GetMemoryData(ResourceAllocator allocator)
{
    ZoneScoped;

    assert(allocator != ResourceAllocator::None);
    DeviceMemory *pMemory = m_Allocators.GetDeviceMemory(allocator);
    return (u8 *)pMemory->m_pMappedMemory;
}

u8 *APIContext::GetBufferMemoryData(Buffer *pBuffer)
{
    ZoneScoped;

    return GetMemoryData(pBuffer->m_Allocator) + pBuffer->m_DataOffset;
}

Image *APIContext::CreateImage(ImageDesc *pDesc)
{
    ZoneScoped;

    Image *pImage = new Image;

    if (pDesc->m_DataSize == 0)
        pDesc->m_DataSize = pDesc->m_Width * pDesc->m_Height * FormatToSize(pDesc->m_Format);

    /// ---------------------------------------------- //

    VkImageCreateInfo imageCreateInfo = {};
    imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.format = (VkFormat)pDesc->m_Format;
    imageCreateInfo.usage = (VkImageUsageFlags)pDesc->m_UsageFlags;
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;

    imageCreateInfo.extent.width = pDesc->m_Width;
    imageCreateInfo.extent.height = pDesc->m_Height;
    imageCreateInfo.extent.depth = 1;

    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageCreateInfo.mipLevels = pDesc->m_MipMapLevels;
    imageCreateInfo.arrayLayers = pDesc->m_ArraySize;
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;

    if (pDesc->m_UsageFlags & ImageUsage::Concurrent)
    {
        imageCreateInfo.sharingMode = VK_SHARING_MODE_CONCURRENT;
        imageCreateInfo.queueFamilyIndexCount = m_pPhysicalDevice->m_SelectedQueueIndices.size();
        imageCreateInfo.pQueueFamilyIndices = &m_pPhysicalDevice->m_SelectedQueueIndices[0];
    }
    else
    {
        imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageCreateInfo.queueFamilyIndexCount = 0;
        imageCreateInfo.pQueueFamilyIndices = nullptr;
    }

    vkCreateImage(m_pDevice, &imageCreateInfo, nullptr, &pImage->m_pHandle);
    AllocateImageMemory(pDesc->m_TargetAllocator, pImage, pDesc->m_DataSize);

    /// INIT BUFFER ///
    pImage->m_Format = pDesc->m_Format;
    pImage->m_Allocator = pDesc->m_TargetAllocator;
    pImage->m_Width = pDesc->m_Width;
    pImage->m_Height = pDesc->m_Height;
    pImage->m_ArraySize = pDesc->m_ArraySize;
    pImage->m_MipMapLevels = pDesc->m_MipMapLevels;

    CreateImageView(pImage, pDesc->m_UsageFlags);

    return pImage;
}

void APIContext::DeleteImage(Image *pImage, bool waitForWork)
{
    ZoneScoped;

    assert(pImage->m_Allocator != ResourceAllocator::None);

    if (waitForWork)
    {
        SwapChainFrame *pCurrentFrame = m_pSwapChain->GetCurrentFrame();
        pCurrentFrame->m_ImageDeleteQueue.push(pImage);
        return;
    }

    DeviceMemory *pMemory = m_Allocators.GetDeviceMemory(pImage->m_Allocator);
    if (pMemory)
        pMemory->Free(pImage->m_AllocatorData);

    if (pImage->m_pViewHandle)
        vkDestroyImageView(m_pDevice, pImage->m_pViewHandle, nullptr);

    if (pImage->m_pHandle)
        vkDestroyImage(m_pDevice, pImage->m_pHandle, nullptr);
}

void APIContext::CreateImageView(Image *pImage, ImageUsage aspectUsage)
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

    vkCreateImageView(m_pDevice, &imageViewCreateInfo, nullptr, &pImage->m_pViewHandle);
}

Sampler *APIContext::CreateSampler(SamplerDesc *pDesc)
{
    ZoneScoped;

    Sampler *pSampler = new Sampler;

    constexpr static VkSamplerAddressMode kAddresModeLUT[] = {
        VK_SAMPLER_ADDRESS_MODE_REPEAT,
        VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
    };

    VkSamplerCreateInfo samplerInfo = {};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.pNext = nullptr;

    samplerInfo.flags = 0;

    samplerInfo.magFilter = (VkFilter)pDesc->m_MinFilter;
    samplerInfo.minFilter = (VkFilter)pDesc->m_MinFilter;

    samplerInfo.addressModeU = kAddresModeLUT[(u32)pDesc->m_AddressU];
    samplerInfo.addressModeV = kAddresModeLUT[(u32)pDesc->m_AddressV];
    samplerInfo.addressModeW = kAddresModeLUT[(u32)pDesc->m_AddressW];

    samplerInfo.anisotropyEnable = pDesc->m_UseAnisotropy;
    samplerInfo.maxAnisotropy = pDesc->m_MaxAnisotropy;

    samplerInfo.mipmapMode = (VkSamplerMipmapMode)pDesc->m_MipFilter;
    samplerInfo.mipLodBias = pDesc->m_MipLODBias;
    samplerInfo.maxLod = pDesc->m_MaxLOD;
    samplerInfo.minLod = pDesc->m_MinLOD;

    samplerInfo.compareEnable = pDesc->m_CompareOp != CompareOp::Never;
    samplerInfo.compareOp = (VkCompareOp)pDesc->m_CompareOp;

    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;

    vkCreateSampler(m_pDevice, &samplerInfo, nullptr, &pSampler->m_pHandle);

    return pSampler;
}

void APIContext::DeleteSampler(VkSampler pSampler)
{
    ZoneScoped;

    vkDestroySampler(m_pDevice, pSampler, nullptr);
}

Semaphore *APIContext::GetAvailableAcquireSemaphore(Semaphore *pOldSemaphore)
{
    ZoneScoped;

    if (m_AcquireSempPool.empty())
    {
        for (u32 i = 0; i < m_AcquireSempPool.max_size(); i++)
        {
            Semaphore *pSemaphore = CreateSemaphore();
            pSemaphore->m_Value = i;  // not timeline value

            SetObjectName(pSemaphore, _FMT("Cached Acquire Sem. {}", m_AcquireSempPool.size()));
            m_AcquireSempPool.push_back(pSemaphore);
        }

        return m_AcquireSempPool.front();
    }

    if (!pOldSemaphore)
        return nullptr;

    u32 idx = (pOldSemaphore->m_Value + 1) % m_AcquireSempPool.size();
    return m_AcquireSempPool[idx];
}

bool APIContext::SetupInstance(BaseWindow *pWindow)
{
    ZoneScoped;

    VkResult vkResult;

    /// Setup Extensions

    LOG_INFO("Checking instance extensions:");

    u32 instanceExtensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtensionCount, nullptr);

    VkExtensionProperties *ppExtensionProp = new VkExtensionProperties[instanceExtensionCount];
    vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtensionCount, ppExtensionProp);

#if _DEBUG
    constexpr u32 debugExtensionCount = 2;
#else
    constexpr u32 debugExtensionCount = 0;
#endif

    constexpr const char *ppRequiredExtensions[] = {
        "VK_KHR_surface",
        "VK_KHR_get_physical_device_properties2",
        "VK_KHR_win32_surface",
#if _DEBUG
        //! Debug extensions, always put it to bottom
        "VK_EXT_debug_utils",
        "VK_EXT_debug_report",
#endif
    };
    eastl::span<const char *const> requiredExtensions = ppRequiredExtensions;

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

        LOG_INFO("\t{}, {}", pExtensionName, found);

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
    createInfo.enabledExtensionCount = requiredExtensions.size();
    createInfo.ppEnabledExtensionNames = requiredExtensions.data();
    createInfo.enabledLayerCount = 0;  //! WE USE VULKAN CONFIGURATOR FOR LAYERS
    createInfo.ppEnabledLayerNames = nullptr;
    vkCreateInstance(&createInfo, nullptr, &m_pInstance);

    return VK::LoadVulkanInstance(m_pInstance);
}

bool APIContext::SetupPhysicalDevice()
{
    ZoneScoped;

    m_pPhysicalDevice = new PhysicalDevice;

    u32 availableDeviceCount = 0;
    vkEnumeratePhysicalDevices(m_pInstance, &availableDeviceCount, nullptr);

    if (availableDeviceCount == 0)
    {
        LOG_CRITICAL("No GPU with Vulkan support.");
        return false;
    }

    VkPhysicalDevice *ppAvailableDevices = new VkPhysicalDevice[availableDeviceCount];
    vkEnumeratePhysicalDevices(m_pInstance, &availableDeviceCount, ppAvailableDevices);

    m_pPhysicalDevice->Init({ ppAvailableDevices, availableDeviceCount });

    return true;
}

bool APIContext::SetupSurface(BaseWindow *pWindow)
{
    ZoneScoped;

    m_pSurface = new Surface;
    m_pSurface->Init(pWindow, m_pInstance, m_pPhysicalDevice);
    return true;
}

bool APIContext::SetupDevice()
{
    ZoneScoped;

    m_pDevice = m_pPhysicalDevice->GetLogicalDevice();

    return VK::LoadVulkanDevice(m_pDevice);
}

}  // namespace lr::Graphics