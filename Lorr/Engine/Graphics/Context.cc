// Created on Monday July 18th 2022 by exdal
// Last modified on Saturday May 20th 2023 by exdal

#include "Context.hh"

#include "Window/Win32/Win32Window.hh"

#include "APIAllocator.hh"
#include "Shader.hh"
#include "VulkanType.hh"

/// DEFINE VULKAN FUNCTIONS
#define _VK_DEFINE_FUNCTION(_name) PFN_##_name _name
_VK_IMPORT_SYMBOLS
_VK_IMPORT_DEVICE_SYMBOLS
_VK_IMPORT_INSTANCE_SYMBOLS
#undef _VK_DEFINE_FUNCTION

namespace lr::Graphics
{
bool Context::Init(APIDesc *pDesc)
{
    ZoneScoped;

    LOG_TRACE("Initializing Vulkan context...");

    constexpr u32 kTypeMem = Memory::MiBToBytes(16);
    APIAllocator::g_Handle.m_TypeAllocator.Init(kTypeMem, 0x2000);
    APIAllocator::g_Handle.m_pTypeData = Memory::Allocate<u8>(kTypeMem);

    if (!LoadVulkan())
        return false;

    if (!SetupInstance(pDesc->m_pTargetWindow))
        return false;

    if (!SetupPhysicalDevice())
        return false;

    if (!SetupSurface(pDesc->m_pTargetWindow))
        return false;

    m_pPhysicalDevice->InitQueueFamilies(m_pSurface);

    if (!SetupDevice())
        return false;

    m_pSwapChain = CreateSwapChain(pDesc->m_pTargetWindow, pDesc->m_SwapChainFlags, nullptr);
    SetObjectName(m_pPhysicalDevice, "Physical Device");
    SetObjectName(m_pSurface, "Default Surface");
    SetObjectName(m_pSwapChain, "Swap Chain");

    InitAllocators(&pDesc->m_AllocatorDesc);

    m_pPipelineCache = CreatePipelineCache();

    m_pDirectQueue = CreateCommandQueue(CommandType::Graphics);
    SetObjectName(m_pDirectQueue, "Direct Queue");
    m_pComputeQueue = CreateCommandQueue(CommandType::Compute);
    SetObjectName(m_pComputeQueue, "Compute Queue");
    m_pTransferQueue = CreateCommandQueue(CommandType::Transfer);
    SetObjectName(m_pTransferQueue, "Transfer Queue");

    LOG_TRACE("Successfully initialized Vulkan API.");

    return true;
}

void Context::InitAllocators(APIAllocatorInitDesc *pDesc)
{
    ZoneScoped;

    m_MADescriptor.Allocator.Init(pDesc->m_DescriptorMem);
    m_MADescriptor.pHeap = CreateHeap(pDesc->m_DescriptorMem, MemoryFlag::HostVisibleCoherent);

    m_MABufferLinear.Allocator.Init(pDesc->m_BufferLinearMem);
    m_MABufferLinear.pHeap = CreateHeap(pDesc->m_BufferLinearMem, MemoryFlag::HostVisibleCoherent);

    for (u32 i = 0; i < m_pSwapChain->m_FrameCount; i++)
    {
        m_MABufferFrametime[i].Allocator.Init(pDesc->m_BufferFrametimeMem);
        m_MABufferFrametime[i].pHeap = CreateHeap(pDesc->m_BufferFrametimeMem, MemoryFlag::HostVisibleCoherent);
    }

    m_MABufferTLSF.Allocator.Init(pDesc->m_BufferTLSFMem, pDesc->m_MaxTLSFAllocations);
    m_MABufferTLSF.pHeap = CreateHeap(pDesc->m_BufferTLSFMem, MemoryFlag::Device);

    m_MABufferTLSFHost.Allocator.Init(pDesc->m_BufferTLSFHostMem, pDesc->m_MaxTLSFAllocations);
    m_MABufferTLSFHost.pHeap = CreateHeap(pDesc->m_BufferTLSFHostMem, MemoryFlag::HostVisibleCoherent);

    m_MAImageTLSF.Allocator.Init(pDesc->m_ImageTLSFMem, pDesc->m_MaxTLSFAllocations);
    m_MAImageTLSF.pHeap = CreateHeap(pDesc->m_ImageTLSFMem, MemoryFlag::Device);
}

u32 Context::GetQueueIndex(CommandType type)
{
    ZoneScoped;

    return m_pPhysicalDevice->GetQueueIndex(type);
}

CommandQueue *Context::CreateCommandQueue(CommandType type)
{
    ZoneScoped;

    CommandQueue *pQueue = new CommandQueue;
    vkGetDeviceQueue(m_pDevice, m_pPhysicalDevice->GetQueueIndex(type), 0, &pQueue->m_pHandle);

    return pQueue;
}

CommandAllocator *Context::CreateCommandAllocator(CommandType type, bool resetLists)
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

CommandList *Context::CreateCommandList(CommandType type, CommandAllocator *pAllocator)
{
    ZoneScoped;

    CommandList *pList = new CommandList;
    pList->m_Type = type;

    if (pAllocator)
        AllocateCommandList(pList, pAllocator);

    return pList;
}

void Context::AllocateCommandList(CommandList *pList, CommandAllocator *pAllocator)
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

void Context::BeginCommandList(CommandList *pList)
{
    ZoneScoped;

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.pNext = nullptr;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(pList->m_pHandle, &beginInfo);
}

void Context::EndCommandList(CommandList *pList)
{
    ZoneScoped;

    vkEndCommandBuffer(pList->m_pHandle);
}

void Context::ResetCommandAllocator(CommandAllocator *pAllocator)
{
    ZoneScoped;

    vkResetCommandPool(m_pDevice, pAllocator->m_pHandle, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT);
}

void Context::Submit(SubmitDesc *pDesc)
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

Fence *Context::CreateFence(bool signaled)
{
    ZoneScoped;

    Fence *pFence = new Fence;

    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = signaled;

    vkCreateFence(m_pDevice, &fenceInfo, nullptr, &pFence->m_pHandle);

    return pFence;
}

void Context::DeleteFence(Fence *pFence)
{
    ZoneScoped;

    vkDestroyFence(m_pDevice, pFence->m_pHandle, nullptr);

    delete pFence;
}

bool Context::IsFenceSignaled(Fence *pFence)
{
    ZoneScoped;

    return vkGetFenceStatus(m_pDevice, pFence->m_pHandle) == VK_SUCCESS;
}

void Context::ResetFence(Fence *pFence)
{
    ZoneScoped;

    vkResetFences(m_pDevice, 1, &pFence->m_pHandle);
}

Semaphore *Context::CreateSemaphore(u32 initialValue, bool binary)
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

void Context::DeleteSemaphore(Semaphore *pSemaphore)
{
    ZoneScoped;

    vkDestroySemaphore(m_pDevice, pSemaphore->m_pHandle, nullptr);
    delete pSemaphore;
}

void Context::WaitForSemaphore(Semaphore *pSemaphore, u64 desiredValue, u64 timeout)
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

VkPipelineCache Context::CreatePipelineCache(u32 initialDataSize, void *pInitialData)
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

PipelineLayout *Context::CreatePipelineLayout(
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

Pipeline *Context::CreateGraphicsPipeline(GraphicsPipelineBuildInfo *pBuildInfo)
{
    ZoneScoped;

    Pipeline *pPipeline = new Pipeline;
    pPipeline->m_BindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    pPipeline->m_pLayout = pBuildInfo->m_pLayout;

    /// BOUND RENDER TARGETS -----------------------------------------------------

    VkFormat pRenderTargetFormats[LR_MAX_COLOR_ATTACHMENT_PER_PASS] = {};

    VkPipelineRenderingCreateInfo renderingInfo = {};
    renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    renderingInfo.pNext = nullptr;
    renderingInfo.viewMask = 0;
    renderingInfo.colorAttachmentCount = pBuildInfo->m_ColorAttachmentFormats.size();
    renderingInfo.pColorAttachmentFormats = pRenderTargetFormats;
    renderingInfo.depthAttachmentFormat = VK::ToFormat(pBuildInfo->m_DepthAttachmentFormat);
    for (u32 i = 0; i < pBuildInfo->m_ColorAttachmentFormats.size(); i++)
        pRenderTargetFormats[i] = VK::ToFormat(pBuildInfo->m_ColorAttachmentFormats[i]);

    /// BOUND SHADER STAGES ------------------------------------------------------

    VkPipelineShaderStageCreateInfo pShaderStageInfos[(u32)ShaderStage::Count] = {};
    for (u32 i = 0; i < pBuildInfo->m_Shaders.size(); i++)
    {
        Shader *pShader = pBuildInfo->m_Shaders[i];

        VkPipelineShaderStageCreateInfo &info = pShaderStageInfos[i];
        info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        info.pName = nullptr;
        info.stage = VK::ToShaderType(pShader->m_Type);
        info.module = pShader->m_pHandle;
        info.pName = "main";
    }

    /// INPUT LAYOUT  ------------------------------------------------------------

    VkPipelineVertexInputStateCreateInfo inputLayoutInfo = {};
    inputLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    inputLayoutInfo.pNext = nullptr;

    VkVertexInputBindingDescription inputBindingInfo = {};
    VkVertexInputAttributeDescription pAttribInfos[LR_MAX_VERTEX_ATTRIBS_PER_PIPELINE] = {};

    InputLayout &inputLayout = pBuildInfo->m_InputLayout;
    if (inputLayout.m_Count != 0)
    {
        inputBindingInfo.binding = 0;
        inputBindingInfo.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        inputBindingInfo.stride = inputLayout.m_Stride;

        for (u32 i = 0; i < inputLayout.m_Count; i++)
        {
            VertexAttrib &element = inputLayout.m_Elements[i];
            VkVertexInputAttributeDescription &attribInfo = pAttribInfos[i];

            attribInfo.binding = 0;
            attribInfo.location = i;
            attribInfo.offset = element.m_Offset;
            attribInfo.format = VK::ToFormat(element.m_Type);
        }

        inputLayoutInfo.vertexAttributeDescriptionCount = inputLayout.m_Count;
        inputLayoutInfo.pVertexAttributeDescriptions = pAttribInfos;
        inputLayoutInfo.vertexBindingDescriptionCount = 1;
        inputLayoutInfo.pVertexBindingDescriptions = &inputBindingInfo;
    }

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
    rasterizerInfo.cullMode = VK::ToCullMode(pBuildInfo->m_SetCullMode);
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

    constexpr static eastl::array<VkDynamicState, 3> kDynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
        VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY,
    };

    VkPipelineDynamicStateCreateInfo dynamicStateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .pNext = nullptr,
        .dynamicStateCount = kDynamicStates.count,
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

Pipeline *Context::CreateComputePipeline(ComputePipelineBuildInfo *pBuildInfo)
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

SwapChain *Context::CreateSwapChain(BaseWindow *pWindow, SwapChainFlag flags, SwapChain *pOldSwapChain)
{
    ZoneScoped;

    SwapChain *pSwapChain = pOldSwapChain;

    if (!pOldSwapChain)
    {
        pSwapChain = new SwapChain;

        VkFormat format = VK::ToFormat(pSwapChain->m_ImageFormat);

        if (!m_pSurface->IsPresentModeSupported(pSwapChain->m_PresentMode))
            flags |= SwapChainFlag::VSync;

        if (flags & SwapChainFlag::VSync)
        {
            pSwapChain->m_PresentMode = VK_PRESENT_MODE_FIFO_KHR;
            pSwapChain->m_FrameCount = 1;
        }
        else if (flags & SwapChainFlag::TripleBuffering)
        {
            pSwapChain->m_FrameCount = 3;
        }

        pSwapChain->m_FrameCount =
            eastl::min(pSwapChain->m_FrameCount, m_pSurface->m_MaxImageCount);  // just to make sure

        if (!m_pSurface->IsFormatSupported(format, pSwapChain->m_ColorSpace))
        {
            LOG_CRITICAL("SwapChain does not support format {}!", format);
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
        .imageFormat = VK::ToFormat(pSwapChain->m_ImageFormat),
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

    VkImage ppSwapChainImages[LR_MAX_FRAME_COUNT] = {};
    vkGetSwapchainImagesKHR(m_pDevice, pSwapChain->m_pHandle, &pSwapChain->m_FrameCount, &ppSwapChainImages[0]);

    for (u32 i = 0; i < pSwapChain->m_FrameCount; i++)
    {
        SwapChainFrame &frame = pSwapChain->m_Frames[i];
        Image *&pImage = frame.m_pImage;

        pImage = new Image;
        pImage->m_pHandle = ppSwapChainImages[i];
        pImage->m_ImageAspect = VK_IMAGE_ASPECT_COLOR_BIT;
        pImage->m_Width = pSwapChain->m_Width;
        pImage->m_Height = pSwapChain->m_Height;
        pImage->m_DataLen = ~0;
        pImage->m_MipMapLevels = 1;
        pImage->m_Format = pSwapChain->m_ImageFormat;
        pImage->m_DeviceDataLen = ~0;
        pImage->m_TargetAllocator = ResourceAllocator::Count;
        CreateImageView(pImage);

        SetObjectName(pImage, Format("Swap Chain Image {}", i));

        frame.m_pPresentSemp = CreateSemaphore();
        SetObjectName(frame.m_pPresentSemp, Format("Present Semaphore {}", i));
    }

    return pSwapChain;
}

void Context::DeleteSwapChain(SwapChain *pSwapChain, bool keepSelf)
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

void Context::ResizeSwapChain(BaseWindow *pWindow)
{
    ZoneScoped;

    WaitForWork();

    DeleteSwapChain(m_pSwapChain, true);
    m_pSwapChain = CreateSwapChain(pWindow, SwapChainFlag::None, m_pSwapChain);
}

SwapChain *Context::GetSwapChain()
{
    return m_pSwapChain;
}

void Context::WaitForWork()
{
    ZoneScoped;

    vkDeviceWaitIdle(m_pDevice);
}

void Context::BeginFrame()
{
    ZoneScoped;

    SwapChainFrame *pCurrentFrame = m_pSwapChain->GetCurrentFrame();
    Semaphore *pSemaphore = GetAvailableAcquireSemaphore(pCurrentFrame->m_pAcquireSemp);

    u32 imageIdx = 0;
    VkResult res = vkAcquireNextImageKHR(
        m_pDevice, m_pSwapChain->m_pHandle, UINT64_MAX, pSemaphore->m_pHandle, nullptr, &imageIdx);
    if (res != VK_SUCCESS)
    {
        assert(!"Uhh please implement swapchain resizing, thanks");
        WaitForWork();  // FUCK
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

    m_MABufferFrametime[m_pSwapChain->m_CurrentFrame].Allocator.Free();
}

void Context::EndFrame()
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
    presentSemaphoreInfo.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

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

Shader *Context::CreateShader(ShaderStage stage, BufferReadStream &buf)
{
    ZoneScoped;

    Shader *pShader = new Shader;
    pShader->m_Type = stage;

    ShaderCompileDesc compileDesc = {};
    compileDesc.m_Type = stage;
#if _DEBUG
    compileDesc.m_Flags = ShaderFlag::GenerateDebugInfo | ShaderFlag::SkipOptimization;
#else
    compileDesc.m_Flags = ShaderFlag::SkipValidation;
#endif

    ShaderCompileOutput shaderOutput = ShaderCompiler::CompileShader(&compileDesc);
    VkShaderModuleCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = shaderOutput.m_DataSpv.size(),
        .pCode = shaderOutput.m_DataSpv.data(),
    };
    vkCreateShaderModule(m_pDevice, &createInfo, nullptr, &pShader->m_pHandle);

    return pShader;
}

Shader *Context::CreateShader(ShaderStage stage, eastl::string_view path)
{
    ZoneScoped;

    FileStream fs(path, false);
    void *pData = fs.ReadAll<void>();
    fs.Close();

    BufferReadStream buf(pData, fs.Size());
    Shader *pShader = CreateShader(stage, buf);

    free(pData);

    return pShader;
}

void Context::DeleteShader(Shader *pShader)
{
    ZoneScoped;

    vkDestroyShaderModule(m_pDevice, pShader->m_pHandle, nullptr);
    delete pShader;
}

DescriptorSetLayout *Context::CreateDescriptorSetLayout(
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
        .flags = createFlags,
        .bindingCount = (u32)elements.size(),
        .pBindings = elements.data(),
    };

    vkCreateDescriptorSetLayout(m_pDevice, &layoutCreateInfo, nullptr, &pLayout->m_pHandle);

    return pLayout;
}

u64 Context::GetDescriptorSetLayoutSize(DescriptorSetLayout *pLayout)
{
    ZoneScoped;

    u64 size = 0;
    vkGetDescriptorSetLayoutSizeEXT(m_pDevice, pLayout->m_pHandle, &size);

    return Memory::AlignUp(size, m_pPhysicalDevice->GetDescriptorBufferAlignment());
}

u64 Context::GetDescriptorSetLayoutBindingOffset(DescriptorSetLayout *pLayout, u32 bindingID)
{
    ZoneScoped;

    u64 offset = 0;
    vkGetDescriptorSetLayoutBindingOffsetEXT(m_pDevice, pLayout->m_pHandle, bindingID, &offset);

    return offset;
}

u64 Context::GetDescriptorSize(DescriptorType type)
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

void Context::GetDescriptorData(
    DescriptorType type, const DescriptorGetInfo &info, u64 dataSize, void *pDataOut)
{
    ZoneScoped;

    VkDescriptorDataEXT descriptorData = { .pUniformTexelBuffer = &info.m_BufferInfo };
    VkDescriptorGetInfoEXT vkInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT,
        .pNext = nullptr,
        .type = VK::ToDescriptorType(type),
        .data = descriptorData,
    };

    vkGetDescriptorEXT(m_pDevice, &vkInfo, dataSize, pDataOut);
}

VkDeviceMemory Context::CreateHeap(u64 heapSize, MemoryFlag flags)
{
    ZoneScoped;

    VkMemoryPropertyFlags memoryProps = 0;
    if (flags & MemoryFlag::Device)
        memoryProps |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    if (flags & MemoryFlag::HostVisible)
        memoryProps |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    if (flags & MemoryFlag::HostCoherent)
        memoryProps |= VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    if (flags & MemoryFlag::HostCached)
        memoryProps |= VK_MEMORY_PROPERTY_HOST_CACHED_BIT;

    VkMemoryAllocateInfo memoryAllocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = heapSize,
        .memoryTypeIndex = m_pPhysicalDevice->GetHeapIndex(memoryProps),
    };

    VkDeviceMemory pHandle = nullptr;
    vkAllocateMemory(m_pDevice, &memoryAllocInfo, nullptr, &pHandle);

    return pHandle;
}

Buffer *Context::CreateBuffer(BufferDesc *pDesc)
{
    ZoneScoped;

    Buffer *pBuffer = new Buffer;
    pBuffer->Init(pDesc);

    VkBufferCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,
        .size = pBuffer->m_DataLen,
        .usage = VK::ToBufferUsage(pDesc->m_UsageFlags) | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
    };
    vkCreateBuffer(m_pDevice, &createInfo, nullptr, &pBuffer->m_pHandle);

    SetAllocator(pBuffer, pBuffer->m_TargetAllocator);
    vkBindBufferMemory(m_pDevice, pBuffer->m_pHandle, pBuffer->m_pMemoryHandle, pBuffer->m_AllocatorOffset);

    VkBufferDeviceAddressInfo deviceAddressInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .pNext = nullptr,
        .buffer = pBuffer->m_pHandle,
    };
    pBuffer->m_DeviceAddress = vkGetBufferDeviceAddress(m_pDevice, &deviceAddressInfo);

    return pBuffer;
}

void Context::DeleteBuffer(Buffer *pBuffer, bool waitForWork)
{
    ZoneScoped;

    if (waitForWork)
    {
        SwapChainFrame *pCurrentFrame = m_pSwapChain->GetCurrentFrame();
        pCurrentFrame->m_BufferDeleteQueue.push(pBuffer);
        return;
    }

    if (pBuffer->m_pMemoryHandle)
    {
        if (pBuffer->m_TargetAllocator == ResourceAllocator::BufferTLSF)
        {
            Memory::TLSFBlock *pBlock = (Memory::TLSFBlock *)pBuffer->m_pAllocatorData;
            assert(pBlock != nullptr);

            m_MABufferTLSF.Allocator.Free(pBlock);
        }
        else if (pBuffer->m_TargetAllocator == ResourceAllocator::None)
        {
            vkFreeMemory(m_pDevice, pBuffer->m_pMemoryHandle, nullptr);
        }
    }

    if (pBuffer->m_pHandle)
        vkDestroyBuffer(m_pDevice, pBuffer->m_pHandle, nullptr);
}

void Context::MapMemory(Buffer *pBuffer, void *&pData, u64 offset, u64 size)
{
    ZoneScoped;

    vkMapMemory(m_pDevice, pBuffer->m_pMemoryHandle, pBuffer->m_AllocatorOffset + offset, size, 0, &pData);
}

void Context::UnmapMemory(Buffer *pBuffer)
{
    ZoneScoped;

    vkUnmapMemory(m_pDevice, pBuffer->m_pMemoryHandle);
}

Image *Context::CreateImage(ImageDesc *pDesc)
{
    ZoneScoped;

    Image *pImage = new Image;
    pImage->Init(pDesc);

    if (pImage->m_DataLen == 0)
        pImage->m_DataLen = pImage->m_Width * pImage->m_Height * GetImageFormatSize(pImage->m_Format);

    /// ---------------------------------------------- //

    VkImageCreateInfo imageCreateInfo = {};
    imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.format = VK::ToFormat(pImage->m_Format);
    imageCreateInfo.usage = VK::ToImageUsage(pDesc->m_UsageFlags);
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;

    imageCreateInfo.extent.width = pImage->m_Width;
    imageCreateInfo.extent.height = pImage->m_Height;
    imageCreateInfo.extent.depth = 1;

    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageCreateInfo.mipLevels = pImage->m_MipMapLevels;
    imageCreateInfo.arrayLayers = pImage->m_ArraySize;
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

    if (pDesc->m_UsageFlags & ImageUsage::AspectColor)
    {
        pImage->m_ImageAspect |= VK_IMAGE_ASPECT_COLOR_BIT;
    }
    else if (pDesc->m_UsageFlags & ImageUsage::AspectDepthStencil)
    {
        pImage->m_ImageAspect |= VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    }

    /// ---------------------------------------------- //

    SetAllocator(pImage, pImage->m_TargetAllocator);

    vkBindImageMemory(m_pDevice, pImage->m_pHandle, pImage->m_pMemoryHandle, pImage->m_AllocatorOffset);

    CreateImageView(pImage);

    return pImage;
}

void Context::DeleteImage(Image *pImage, bool waitForWork)
{
    ZoneScoped;

    if (waitForWork)
    {
        SwapChainFrame *pCurrentFrame = m_pSwapChain->GetCurrentFrame();
        pCurrentFrame->m_ImageDeleteQueue.push(pImage);
        return;
    }

    if (pImage->m_pMemoryHandle)
    {
        if (pImage->m_TargetAllocator == ResourceAllocator::ImageTLSF)
        {
            Memory::TLSFBlock *pBlock = (Memory::TLSFBlock *)pImage->m_pAllocatorData;
            assert(pBlock != nullptr);

            m_MAImageTLSF.Allocator.Free(pBlock);
        }
        else if (pImage->m_TargetAllocator == ResourceAllocator::None)
        {
            vkFreeMemory(m_pDevice, pImage->m_pMemoryHandle, nullptr);
        }
    }

    if (pImage->m_pViewHandle)
        vkDestroyImageView(m_pDevice, pImage->m_pViewHandle, nullptr);

    if (pImage->m_pHandle)
        vkDestroyImage(m_pDevice, pImage->m_pHandle, nullptr);
}

void Context::CreateImageView(Image *pImage)
{
    ZoneScoped;

    VkImageViewCreateInfo imageViewCreateInfo = {};
    imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;

    imageViewCreateInfo.image = pImage->m_pHandle;
    imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    imageViewCreateInfo.format = VK::ToFormat(pImage->m_Format);

    imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    imageViewCreateInfo.subresourceRange.aspectMask = pImage->m_ImageAspect;
    imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
    imageViewCreateInfo.subresourceRange.levelCount = pImage->m_MipMapLevels;
    imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
    imageViewCreateInfo.subresourceRange.layerCount = 1;

    vkCreateImageView(m_pDevice, &imageViewCreateInfo, nullptr, &pImage->m_pViewHandle);
}

Sampler *Context::CreateSampler(SamplerDesc *pDesc)
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

void Context::DeleteSampler(VkSampler pSampler)
{
    ZoneScoped;

    vkDestroySampler(m_pDevice, pSampler, nullptr);
}

void Context::SetAllocator(Buffer *pBuffer, ResourceAllocator targetAllocator)
{
    ZoneScoped;

    VkMemoryRequirements memoryRequirements = {};
    vkGetBufferMemoryRequirements(m_pDevice, pBuffer->m_pHandle, &memoryRequirements);

    pBuffer->m_DeviceDataLen = memoryRequirements.size;

    switch (targetAllocator)
    {
        case ResourceAllocator::Descriptor:
        {
            pBuffer->m_AllocatorOffset =
                m_MADescriptor.Allocator.Allocate(pBuffer->m_DeviceDataLen, memoryRequirements.alignment);
            pBuffer->m_pMemoryHandle = m_MADescriptor.pHeap;

            break;
        }
        case ResourceAllocator::BufferLinear:
        {
            pBuffer->m_AllocatorOffset =
                m_MABufferLinear.Allocator.Allocate(pBuffer->m_DeviceDataLen, memoryRequirements.alignment);
            pBuffer->m_pMemoryHandle = m_MABufferLinear.pHeap;

            break;
        }
        case ResourceAllocator::BufferTLSF:
        {
            Memory::TLSFBlock *pBlock =
                m_MABufferTLSF.Allocator.Allocate(pBuffer->m_DeviceDataLen, memoryRequirements.alignment);

            pBuffer->m_pAllocatorData = pBlock;
            pBuffer->m_AllocatorOffset = pBlock->m_Offset;
            pBuffer->m_pMemoryHandle = m_MABufferTLSF.pHeap;

            break;
        }
        case ResourceAllocator::BufferTLSF_Host:
        {
            Memory::TLSFBlock *pBlock =
                m_MABufferTLSFHost.Allocator.Allocate(pBuffer->m_DeviceDataLen, memoryRequirements.alignment);

            pBuffer->m_pAllocatorData = pBlock;
            pBuffer->m_AllocatorOffset = pBlock->m_Offset;
            pBuffer->m_pMemoryHandle = m_MABufferTLSFHost.pHeap;

            break;
        }
        case ResourceAllocator::BufferFrametime:
        {
            u32 currentFrame = m_pSwapChain->m_CurrentFrame;
            pBuffer->m_AllocatorOffset = m_MABufferFrametime[currentFrame].Allocator.Allocate(
                pBuffer->m_DeviceDataLen, memoryRequirements.alignment);
            pBuffer->m_pMemoryHandle = m_MABufferFrametime[currentFrame].pHeap;

            break;
        }
        default:
            LOG_CRITICAL("Trying to allocate VKBuffer resource from invalid allocator.");
            break;
    }
}

void Context::SetAllocator(Image *pImage, ResourceAllocator targetAllocator)
{
    ZoneScoped;

    VkMemoryRequirements memoryRequirements = {};
    vkGetImageMemoryRequirements(m_pDevice, pImage->m_pHandle, &memoryRequirements);

    pImage->m_DeviceDataLen = memoryRequirements.size;

    switch (targetAllocator)
    {
        case ResourceAllocator::None:
        {
            pImage->m_pMemoryHandle = CreateHeap(
                Memory::AlignUp(memoryRequirements.size, memoryRequirements.alignment), MemoryFlag::Device);

            break;
        }
        case ResourceAllocator::ImageTLSF:
        {
            Memory::TLSFBlock *pBlock =
                m_MAImageTLSF.Allocator.Allocate(pImage->m_DataLen, memoryRequirements.alignment);

            pImage->m_pAllocatorData = pBlock;
            pImage->m_AllocatorOffset = pBlock->m_Offset;
            pImage->m_pMemoryHandle = m_MAImageTLSF.pHeap;

            break;
        }
        default:
            break;
    }
}

Semaphore *Context::GetAvailableAcquireSemaphore(Semaphore *pOldSemaphore)
{
    ZoneScoped;

    if (m_AcquireSempPool.empty())
    {
        for (u32 i = 0; i < m_AcquireSempPool.max_size(); i++)
        {
            Semaphore *pSemaphore = CreateSemaphore();
            pSemaphore->m_Value = i;  // not timeline value

            SetObjectName(pSemaphore, Format("Cached Acquire Sem. {}", m_AcquireSempPool.size()));
            m_AcquireSempPool.push_back(pSemaphore);
        }

        return m_AcquireSempPool.front();
    }

    if (!pOldSemaphore)
        return nullptr;

    u32 idx = (pOldSemaphore->m_Value + 1) % m_AcquireSempPool.size();
    return m_AcquireSempPool[idx];
}

bool Context::LoadVulkan()
{
    m_VulkanLib = LoadLibraryA("vulkan-1.dll");

    if (m_VulkanLib == NULL)
    {
        LOG_CRITICAL("Failed to find Vulkan dynamic library.");
        return false;
    }

#define _VK_DEFINE_FUNCTION(_name)                                 \
    _name = (PFN_##_name)GetProcAddress(m_VulkanLib, #_name);      \
    if (_name == nullptr)                                          \
    {                                                              \
        LOG_CRITICAL("Cannot load Vulkan function '{}'!", #_name); \
        return false;                                              \
    }

    _VK_IMPORT_SYMBOLS
#undef _VK_DEFINE_FUNCTION

    return true;
}

bool Context::SetupInstance(BaseWindow *pWindow)
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

    constexpr eastl::array<const char *, 3 + debugExtensionCount> ppRequiredExtensions = {
        "VK_KHR_surface",
        "VK_KHR_get_physical_device_properties2",
        "VK_KHR_win32_surface",
#if _DEBUG
        //! Debug extensions, always put it to bottom
        "VK_EXT_debug_utils",
        "VK_EXT_debug_report",
#endif
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
    createInfo.enabledExtensionCount = ppRequiredExtensions.count;
    createInfo.ppEnabledExtensionNames = ppRequiredExtensions.data();
    createInfo.enabledLayerCount = 0;  //! WE USE VULKAN CONFIGURATOR FOR LAYERS
    createInfo.ppEnabledLayerNames = nullptr;
    vkCreateInstance(&createInfo, nullptr, &m_pInstance);

    /// LOAD INSTANCE EXCLUSIVE FUNCTIONS ///

#define _VK_DEFINE_FUNCTION(_name)                                          \
    _name = (PFN_##_name)vkGetInstanceProcAddr(m_pInstance, #_name);        \
    if (_name == nullptr)                                                   \
    {                                                                       \
        LOG_CRITICAL("Cannot load Vulkan Instance function '{}'!", #_name); \
        return false;                                                       \
    }

    _VK_IMPORT_INSTANCE_SYMBOLS
#undef _VK_DEFINE_FUNCTION

    return true;
}

bool Context::SetupPhysicalDevice()
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

bool Context::SetupSurface(BaseWindow *pWindow)
{
    ZoneScoped;

    m_pSurface = new Surface;
    m_pSurface->Init(pWindow, m_pInstance, m_pPhysicalDevice);
    return true;
}

bool Context::SetupDevice()
{
    ZoneScoped;

    m_pDevice = m_pPhysicalDevice->GetLogicalDevice();

    /// LOAD DEVICE EXCLUSIVE FUNCTIONS ///

#define _VK_DEFINE_FUNCTION(_name)                                          \
    _name = (PFN_##_name)vkGetDeviceProcAddr(m_pDevice, #_name);            \
    if (_name == nullptr)                                                   \
    {                                                                       \
        LOG_CRITICAL("Cannot load Vulkan Instance function '{}'!", #_name); \
        return false;                                                       \
    }

    _VK_IMPORT_DEVICE_SYMBOLS
#undef _VK_DEFINE_FUNCTION

    return true;
}

}  // namespace lr::Graphics