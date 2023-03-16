#include "VKAPI.hh"

#include "Core/Window/Win32/Win32Window.hh"

#include "Allocator.hh"
#include "VKShader.hh"

/// DEFINE VULKAN FUNCTIONS
#define _VK_DEFINE_FUNCTION(_name) PFN_##_name _name
_VK_IMPORT_SYMBOLS
_VK_IMPORT_INSTANCE_SYMBOLS
#undef _VK_DEFINE_FUNCTION

#define VKCall(fn, err)                            \
    if ((vkResult = fn) != VK_SUCCESS)             \
    {                                              \
        LOG_CRITICAL(err " - Code: {}", vkResult); \
    }

#define VKCallRet(fn, err, ret)                    \
    if ((vkResult = fn) != VK_SUCCESS)             \
    {                                              \
        LOG_CRITICAL(err " - Code: {}", vkResult); \
        return ret;                                \
    }

namespace lr::Graphics
{
bool VKAPI::Init(APIDesc *pDesc)
{
    ZoneScoped;

    LOG_TRACE("Initializing Vulkan context...");

    LoadVulkan();
    SetupInstance(pDesc->m_pTargetWindow);

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

        if (!deviceFeatures.tessellationShader || !deviceFeatures.geometryShader
            || !deviceFeatures.fullDrawIndexUint32)
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
    vkGetPhysicalDeviceSurfaceFormatsKHR(
        m_pPhysicalDevice, m_pSurface, &m_ValidSurfaceFormatCount, m_pValidSurfaceFormats);

    /// Get surface present modes
    vkGetPhysicalDeviceSurfacePresentModesKHR(m_pPhysicalDevice, m_pSurface, &m_ValidPresentModeCount, nullptr);

    m_pValidPresentModes = new VkPresentModeKHR[m_ValidPresentModeCount];
    vkGetPhysicalDeviceSurfacePresentModesKHR(
        m_pPhysicalDevice, m_pSurface, &m_ValidPresentModeCount, m_pValidPresentModes);

    vkGetPhysicalDeviceMemoryProperties(m_pPhysicalDevice, &m_MemoryProps);

    // yeah uh, dont delete
    // delete[] ppAvailableDevices;

    InitAllocators(&pDesc->m_AllocatorDesc);

    m_pDescriptorPool = CreateDescriptorPool({
        { LR_DESCRIPTOR_TYPE_SHADER_RESOURCE_IMAGE, 256 },
        { LR_DESCRIPTOR_TYPE_CONSTANT_BUFFER, 256 },
        { LR_DESCRIPTOR_TYPE_SAMPLER, 256 },
    });

    m_pPipelineCache = CreatePipelineCache();

    m_pDirectQueue = CreateCommandQueue(LR_COMMAND_LIST_TYPE_GRAPHICS);
    SetObjectName(m_pDirectQueue, "Direct Queue");
    m_pComputeQueue = CreateCommandQueue(LR_COMMAND_LIST_TYPE_COMPUTE);
    SetObjectName(m_pComputeQueue, "Compute Queue");
    m_pTransferQueue = CreateCommandQueue(LR_COMMAND_LIST_TYPE_TRANSFER);
    SetObjectName(m_pTransferQueue, "Transfer Queue");

    m_SwapChain.Init(pDesc->m_pTargetWindow, this, pDesc->m_SwapChainFlags);
    SetObjectName(&m_SwapChain, "Swap Chain");
    m_SwapChain.CreateBackBuffers(this);

    ShaderCompiler::Init();

    LOG_TRACE("Successfully initialized Vulkan API.");

    return true;
}

void VKAPI::InitAllocators(APIAllocatorInitDesc *pDesc)
{
    ZoneScoped;

    constexpr u32 kTypeMem = Memory::MiBToBytes(16);

    APIAllocator::g_Handle.m_TypeAllocator.Init(kTypeMem, 0x2000);
    APIAllocator::g_Handle.m_pTypeData = Memory::Allocate<u8>(kTypeMem);

    m_MADescriptor.Allocator.Init(pDesc->m_DescriptorMem);
    m_MADescriptor.pHeap = CreateHeap(pDesc->m_DescriptorMem, true);

    m_MABufferLinear.Allocator.Init(pDesc->m_BufferLinearMem);
    m_MABufferLinear.pHeap = CreateHeap(pDesc->m_BufferLinearMem, true);

    for (u32 i = 0; i < m_SwapChain.m_FrameCount; i++)
    {
        m_MABufferFrametime[i].Allocator.Init(pDesc->m_BufferFrametimeMem);
        m_MABufferFrametime[i].pHeap = CreateHeap(pDesc->m_BufferFrametimeMem, true);
    }

    m_MABufferTLSF.Allocator.Init(pDesc->m_BufferTLSFMem, pDesc->m_MaxTLSFAllocations);
    m_MABufferTLSF.pHeap = CreateHeap(pDesc->m_BufferTLSFMem, false);

    m_MABufferTLSFHost.Allocator.Init(pDesc->m_BufferTLSFHostMem, pDesc->m_MaxTLSFAllocations);
    m_MABufferTLSFHost.pHeap = CreateHeap(pDesc->m_BufferTLSFHostMem, true);

    m_MAImageTLSF.Allocator.Init(pDesc->m_ImageTLSFMem, pDesc->m_MaxTLSFAllocations);
    m_MAImageTLSF.pHeap = CreateHeap(pDesc->m_ImageTLSFMem, false);
}

CommandQueue *VKAPI::CreateCommandQueue(CommandListType type)
{
    ZoneScoped;

    CommandQueue *pQueue = new CommandQueue;

    VkQueue pHandle = nullptr;
    vkGetDeviceQueue(m_pDevice, m_QueueInfo.m_Indexes[type], 0, &pHandle);

    pQueue->Init(pHandle);

    return pQueue;
}

CommandAllocator *VKAPI::CreateCommandAllocator(CommandListType type, bool resetLists)
{
    ZoneScoped;

    CommandAllocator *pAllocator = new CommandAllocator;

    VkCommandPoolCreateInfo allocatorInfo = {};
    allocatorInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    allocatorInfo.flags = resetLists ? VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT : 0;
    allocatorInfo.queueFamilyIndex = m_QueueInfo.m_Indexes[type];

    vkCreateCommandPool(m_pDevice, &allocatorInfo, nullptr, &pAllocator->m_pHandle);

    return pAllocator;
}

CommandList *VKAPI::CreateCommandList(CommandListType type, CommandAllocator *pAllocator)
{
    ZoneScoped;

    CommandList *pList = new CommandList;
    pList->m_Type = type;
    pList->m_pQueueInfo = &m_QueueInfo;

    if (pAllocator)
        AllocateCommandList(pList, pAllocator);

    return pList;
}

void VKAPI::AllocateCommandList(CommandList *pList, CommandAllocator *pAllocator)
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

void VKAPI::BeginCommandList(CommandList *pList)
{
    ZoneScoped;

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.pNext = nullptr;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(pList->m_pHandle, &beginInfo);
}

void VKAPI::EndCommandList(CommandList *pList)
{
    ZoneScoped;

    vkEndCommandBuffer(pList->m_pHandle);
}

void VKAPI::ResetCommandAllocator(CommandAllocator *pAllocator)
{
    ZoneScoped;

    vkResetCommandPool(m_pDevice, pAllocator->m_pHandle, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT);
}

u32 VKAPI::GetQueueIndex(CommandListType type)
{
    ZoneScoped;

    if (type == LR_COMMAND_LIST_TYPE_COUNT)
        return VK_QUEUE_FAMILY_IGNORED;

    return m_QueueInfo.m_Indexes[type];
}

void VKAPI::Submit(SubmitDesc *pDesc)
{
    ZoneScoped;

    VkQueue pQueueHandle = nullptr;
    switch (pDesc->m_Type)
    {
        case LR_COMMAND_LIST_TYPE_GRAPHICS:
            pQueueHandle = m_pDirectQueue->m_pHandle;
            break;
        case LR_COMMAND_LIST_TYPE_COMPUTE:
            pQueueHandle = m_pComputeQueue->m_pHandle;
            break;
        case LR_COMMAND_LIST_TYPE_TRANSFER:
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

Fence *VKAPI::CreateFence(bool signaled)
{
    ZoneScoped;

    Fence *pFence = new Fence;

    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = signaled;

    vkCreateFence(m_pDevice, &fenceInfo, nullptr, &pFence->m_pHandle);

    return pFence;
}

void VKAPI::DeleteFence(Fence *pFence)
{
    ZoneScoped;

    vkDestroyFence(m_pDevice, pFence->m_pHandle, nullptr);

    delete pFence;
}

bool VKAPI::IsFenceSignaled(Fence *pFence)
{
    ZoneScoped;

    return vkGetFenceStatus(m_pDevice, pFence->m_pHandle) == VK_SUCCESS;
}

void VKAPI::ResetFence(Fence *pFence)
{
    ZoneScoped;

    vkResetFences(m_pDevice, 1, &pFence->m_pHandle);
}

Semaphore *VKAPI::CreateSemaphore(u32 initialValue, bool binary)
{
    ZoneScoped;

    Semaphore *pSemaphore = new Semaphore;
    pSemaphore->m_Value = initialValue;

    VkSemaphoreTypeCreateInfo semaphoreTypeInfo = {};
    semaphoreTypeInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
    semaphoreTypeInfo.semaphoreType = binary ? VK_SEMAPHORE_TYPE_BINARY : VK_SEMAPHORE_TYPE_TIMELINE;
    semaphoreTypeInfo.initialValue = pSemaphore->m_Value;

    VkSemaphoreCreateInfo semaphoreInfo = {};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphoreInfo.pNext = &semaphoreTypeInfo;

    vkCreateSemaphore(m_pDevice, &semaphoreInfo, nullptr, &pSemaphore->m_pHandle);

    return pSemaphore;
}

void VKAPI::DeleteSemaphore(Semaphore *pSemaphore)
{
    ZoneScoped;

    vkDestroySemaphore(m_pDevice, pSemaphore->m_pHandle, nullptr);
    delete pSemaphore;
}

void VKAPI::WaitForSemaphore(Semaphore *pSemaphore, u64 desiredValue, u64 timeout)
{
    ZoneScoped;

    VkSemaphoreWaitInfo waitInfo = {};
    waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
    waitInfo.semaphoreCount = 1;
    waitInfo.pSemaphores = &pSemaphore->m_pHandle;
    waitInfo.pValues = &desiredValue;

    vkWaitSemaphores(m_pDevice, &waitInfo, timeout);
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

VkPipelineLayout VKAPI::SerializePipelineLayout(PipelineLayoutSerializeDesc *pDesc, Pipeline *pPipeline)
{
    ZoneScoped;

    VkDescriptorSetLayout pSetLayouts[LR_MAX_DESCRIPTOR_SETS_PER_PIPELINE] = {};

    for (u32 i = 0; i < pDesc->m_Layouts.size(); i++)
        pSetLayouts[i] = pDesc->m_Layouts[i]->m_pHandle;

    VkPushConstantRange pPushConstants[LR_MAX_PUSH_CONSTANTS_PER_PIPELINE] = {};

    for (u32 i = 0; i < pDesc->m_PushConstants.size(); i++)
    {
        PushConstantDesc &pushConstant = pDesc->m_PushConstants[i];

        pPushConstants[i].stageFlags = ToVKShaderType(pushConstant.m_Stage);
        pPushConstants[i].offset = pushConstant.m_Offset;
        pPushConstants[i].size = pushConstant.m_Size;
    }

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.setLayoutCount = pDesc->m_Layouts.size();
    pipelineLayoutCreateInfo.pSetLayouts = pSetLayouts;
    pipelineLayoutCreateInfo.pushConstantRangeCount = pDesc->m_PushConstants.size();
    pipelineLayoutCreateInfo.pPushConstantRanges = pPushConstants;

    VkPipelineLayout pLayout = nullptr;
    vkCreatePipelineLayout(m_pDevice, &pipelineLayoutCreateInfo, nullptr, &pLayout);

    return pLayout;
}

Pipeline *VKAPI::CreateGraphicsPipeline(GraphicsPipelineBuildInfo *pBuildInfo)
{
    ZoneScoped;

    Pipeline *pPipeline = new Pipeline;
    pPipeline->m_BindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

    constexpr VkBlendFactor kBlendFactorLUT[] = {
        VK_BLEND_FACTOR_ZERO,                      // Zero
        VK_BLEND_FACTOR_ONE,                       // One
        VK_BLEND_FACTOR_SRC_COLOR,                 // SrcColor
        VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR,       // InvSrcColor
        VK_BLEND_FACTOR_SRC_ALPHA,                 // SrcAlpha
        VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,       // InvSrcAlpha
        VK_BLEND_FACTOR_DST_ALPHA,                 // DestAlpha
        VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA,       // InvDestAlpha
        VK_BLEND_FACTOR_DST_COLOR,                 // DestColor
        VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR,       // InvDestColor
        VK_BLEND_FACTOR_SRC_ALPHA_SATURATE,        // SrcAlphaSat
        VK_BLEND_FACTOR_CONSTANT_COLOR,            // ConstantColor
        VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR,  // InvConstantColor
        VK_BLEND_FACTOR_SRC1_COLOR,                // Src1Color
        VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR,      // InvSrc1Color
        VK_BLEND_FACTOR_SRC1_ALPHA,                // Src1Alpha
        VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA,      // InvSrc1Alpha
    };

    /// PIPELINE LAYOUT ----------------------------------------------------------

    PipelineLayoutSerializeDesc layoutDesc = {
        .m_Layouts = pBuildInfo->m_Layouts,
        .m_PushConstants = pBuildInfo->m_PushConstants,
    };

    VkPipelineLayout pLayout = SerializePipelineLayout(&layoutDesc, pPipeline);
    pPipeline->m_pLayout = pLayout;

    /// BOUND RENDER TARGETS -----------------------------------------------------

    VkFormat pRenderTargetFormats[LR_MAX_RENDER_TARGET_PER_PASS] = {};

    VkPipelineRenderingCreateInfo renderingInfo = {};
    renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    renderingInfo.pNext = nullptr;
    renderingInfo.viewMask = 0;
    renderingInfo.colorAttachmentCount = pBuildInfo->m_ColorAttachmentFormats.size();
    renderingInfo.pColorAttachmentFormats = pRenderTargetFormats;
    renderingInfo.depthAttachmentFormat = ToVKFormat(pBuildInfo->m_DepthAttachmentFormat);
    for (u32 i = 0; i < pBuildInfo->m_ColorAttachmentFormats.size(); i++)
        pRenderTargetFormats[i] = ToVKFormat(pBuildInfo->m_ColorAttachmentFormats[i]);

    /// BOUND SHADER STAGES ------------------------------------------------------

    VkPipelineShaderStageCreateInfo pShaderStageInfos[LR_SHADER_STAGE_COUNT] = {};
    for (u32 i = 0; i < pBuildInfo->m_Shaders.size(); i++)
    {
        Shader *pShader = pBuildInfo->m_Shaders[i];

        VkPipelineShaderStageCreateInfo &info = pShaderStageInfos[i];
        info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        info.pName = nullptr;
        info.stage = ToVKShaderType(pShader->m_Type);
        info.module = pShader->m_pHandle;
        info.pName = "main";
    }

    /// INPUT LAYOUT  ------------------------------------------------------------

    VkPipelineVertexInputStateCreateInfo inputLayoutInfo = {};
    inputLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    inputLayoutInfo.pNext = nullptr;

    VkVertexInputBindingDescription inputBindingInfo = {};
    VkVertexInputAttributeDescription pAttribInfos[LR_MAX_VERTEX_ATTRIBS_PER_PIPELINE] = {};

    InputLayout *pInputLayout = pBuildInfo->m_pInputLayout;
    if (pInputLayout)
    {
        inputBindingInfo.binding = 0;
        inputBindingInfo.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        inputBindingInfo.stride = pInputLayout->m_Stride;

        for (u32 i = 0; i < pInputLayout->m_Count; i++)
        {
            VertexAttrib &element = pInputLayout->m_Elements[i];
            VkVertexInputAttributeDescription &attribInfo = pAttribInfos[i];

            attribInfo.binding = 0;
            attribInfo.location = i;
            attribInfo.offset = element.m_Offset;
            attribInfo.format = ToVKFormat(element.m_Type);
        }

        inputLayoutInfo.vertexAttributeDescriptionCount = pInputLayout->m_Count;
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
    rasterizerInfo.cullMode = ToVKCullMode(pBuildInfo->m_SetCullMode);
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

    VkPipelineColorBlendAttachmentState pBlendAttachmentInfos[LR_MAX_RENDER_TARGET_PER_PASS] = {};

    VkPipelineColorBlendStateCreateInfo colorBlendInfo = {};
    colorBlendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlendInfo.pNext = nullptr;
    colorBlendInfo.logicOpEnable = false;
    colorBlendInfo.attachmentCount = pBuildInfo->m_ColorAttachmentFormats.size();
    colorBlendInfo.pAttachments = pBlendAttachmentInfos;

    for (u32 i = 0; i < pBuildInfo->m_ColorAttachmentFormats.size(); i++)
    {
        ColorBlendAttachment &attachment = pBuildInfo->m_BlendAttachments[i];
        VkPipelineColorBlendAttachmentState &renderTarget = pBlendAttachmentInfos[i];

        renderTarget.blendEnable = attachment.m_BlendEnable;
        renderTarget.colorWriteMask = attachment.m_WriteMask;
        renderTarget.srcColorBlendFactor = kBlendFactorLUT[attachment.m_SrcBlend];
        renderTarget.dstColorBlendFactor = kBlendFactorLUT[attachment.m_DstBlend];
        renderTarget.srcAlphaBlendFactor = kBlendFactorLUT[attachment.m_SrcBlendAlpha];
        renderTarget.dstAlphaBlendFactor = kBlendFactorLUT[attachment.m_DstBlendAlpha];
        renderTarget.colorBlendOp = (VkBlendOp)attachment.m_ColorBlendOp;
        renderTarget.alphaBlendOp = (VkBlendOp)attachment.m_AlphaBlendOp;
    }

    /// DYNAMIC STATE ------------------------------------------------------------

    constexpr static eastl::array<VkDynamicState, 3> kDynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
        VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY,
    };

    VkPipelineDynamicStateCreateInfo dynamicStateInfo = {};
    dynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicStateInfo.pNext = nullptr;
    dynamicStateInfo.dynamicStateCount = kDynamicStates.count;
    dynamicStateInfo.pDynamicStates = kDynamicStates.data();

    /// GRAPHICS PIPELINE --------------------------------------------------------

    VkGraphicsPipelineCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    createInfo.pNext = &renderingInfo;
    createInfo.stageCount = pBuildInfo->m_Shaders.size();
    createInfo.pStages = pShaderStageInfos;
    createInfo.pVertexInputState = &inputLayoutInfo;
    createInfo.pInputAssemblyState = &inputAssemblyInfo;
    createInfo.pTessellationState = &tessellationInfo;
    createInfo.pViewportState = &viewportInfo;
    createInfo.pRasterizationState = &rasterizerInfo;
    createInfo.pMultisampleState = &multisampleInfo;
    createInfo.pDepthStencilState = &depthStencilInfo;
    createInfo.pColorBlendState = &colorBlendInfo;
    createInfo.pDynamicState = &dynamicStateInfo;
    createInfo.layout = pLayout;

    vkCreateGraphicsPipelines(m_pDevice, m_pPipelineCache, 1, &createInfo, nullptr, &pPipeline->m_pHandle);

    return pPipeline;
}

Pipeline *VKAPI::CreateComputePipeline(ComputePipelineBuildInfo *pBuildInfo)
{
    ZoneScoped;

    Pipeline *pPipeline = new Pipeline;
    pPipeline->m_BindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;

    PipelineLayoutSerializeDesc layoutDesc = {
        .m_Layouts = pBuildInfo->m_Layouts,
        .m_PushConstants = pBuildInfo->m_PushConstants,
    };

    VkPipelineLayout pLayout = SerializePipelineLayout(&layoutDesc, pPipeline);
    pPipeline->m_pLayout = pLayout;

    VkComputePipelineCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    createInfo.pNext = nullptr;
    createInfo.layout = pLayout;

    VkPipelineShaderStageCreateInfo &shaderStage = createInfo.stage;
    shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStage.pNext = nullptr;
    shaderStage.flags = 0;
    shaderStage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    shaderStage.pName = "main";
    shaderStage.module = pBuildInfo->m_pShader->m_pHandle;
    shaderStage.pSpecializationInfo = nullptr;

    vkCreateComputePipelines(m_pDevice, m_pPipelineCache, 1, &createInfo, nullptr, &pPipeline->m_pHandle);

    return pPipeline;
}

void VKAPI::CreateSwapChain(VkSwapchainKHR &pHandle, VkSwapchainCreateInfoKHR &info)
{
    ZoneScoped;

    VkResult vkResult = {};
    VKCall(vkCreateSwapchainKHR(m_pDevice, &info, nullptr, &pHandle), "Failed to create Vulkan SwapChain.");
}

void VKAPI::DeleteSwapChain(SwapChain *pSwapChain)
{
    ZoneScoped;

    vkDestroySwapchainKHR(m_pDevice, pSwapChain->m_pHandle, nullptr);
}

void VKAPI::GetSwapChainImages(VkSwapchainKHR &pHandle, u32 bufferCount, VkImage *pImages)
{
    ZoneScoped;

    vkGetSwapchainImagesKHR(m_pDevice, pHandle, &bufferCount, pImages);
}

void VKAPI::ResizeSwapChain(u32 width, u32 height)
{
    ZoneScoped;

    WaitForWork();

    m_SwapChain.m_Width = width;
    m_SwapChain.m_Height = height;

    m_SwapChain.DestroyHandle(this);
    m_SwapChain.CreateHandle(this);
    m_SwapChain.CreateBackBuffers(this);
}

SwapChain *VKAPI::GetSwapChain()
{
    return &m_SwapChain;
}

SwapChainFrame *VKAPI::GetCurrentFrame()
{
    return &m_SwapChain.m_pFrames[m_SwapChain.m_CurrentFrame];
}

void VKAPI::WaitForWork()
{
    ZoneScoped;

    vkDeviceWaitIdle(m_pDevice);
}

void VKAPI::BeginFrame()
{
    ZoneScoped;

    VkSwapchainKHR pSwapChain = m_SwapChain.m_pHandle;
    SwapChainFrame *pCurrentFrame = GetCurrentFrame();
    Semaphore *pSemaphore = GetAvailableAcquireSemaphore(pCurrentFrame->m_pAcquireSemp);

    u32 imageIdx = 0;
    VkResult res =
        vkAcquireNextImageKHR(m_pDevice, pSwapChain, UINT64_MAX, pSemaphore->m_pHandle, nullptr, &imageIdx);
    if (res != VK_SUCCESS)
    {
        assert(!"why?");
        WaitForWork();  // FUCK
    }

    m_SwapChain.Advance(imageIdx, pSemaphore);

    /// PAST FRAME WORK ///
    pCurrentFrame = GetCurrentFrame();

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

    m_MABufferFrametime[m_SwapChain.m_CurrentFrame].Allocator.Free();
}

void VKAPI::EndFrame()
{
    ZoneScoped;

    SwapChainFrame *pCurrentFrame = GetCurrentFrame();

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
    presentInfo.pSwapchains = &m_SwapChain.m_pHandle;
    presentInfo.pImageIndices = &m_SwapChain.m_CurrentFrame;
    vkQueuePresentKHR(pQueue, &presentInfo);
}

Shader *VKAPI::CreateShader(ShaderStage stage, BufferReadStream &buf)
{
    ZoneScoped;

    Shader *pShader = new Shader;
    pShader->m_Type = stage;

    ShaderCompileDesc compileDesc = {};
    compileDesc.m_Type = stage;
#if _DEBUG
    compileDesc.m_Flags = LR_SHADER_FLAG_GENERATE_DEBUG_INFO | LR_SHADER_FLAG_SKIP_OPTIMIZATION;
#else
    compileDesc.m_Flags = LR_SHADER_FLAG_SKIP_VALIDATION;
#endif

    u32 *pModuleData = nullptr;
    u32 moduleSize = 0;
    ShaderCompiler::CompileShader(&compileDesc, buf, pModuleData, moduleSize);

    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.pCode = pModuleData;
    createInfo.codeSize = moduleSize;
    vkCreateShaderModule(m_pDevice, &createInfo, nullptr, &pShader->m_pHandle);

    return pShader;
}

Shader *VKAPI::CreateShader(ShaderStage stage, eastl::string_view path)
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

void VKAPI::DeleteShader(Shader *pShader)
{
    ZoneScoped;

    vkDestroyShaderModule(m_pDevice, pShader->m_pHandle, nullptr);
    delete pShader;
}

DescriptorSetLayout *VKAPI::CreateDescriptorSetLayout(eastl::span<DescriptorLayoutElement> elements)
{
    ZoneScoped;

    u64 hash = 0;
    DescriptorSetLayout *pLayout = nullptr;
    if ((pLayout = m_LayoutCache.Get(elements, hash)))
        return pLayout;

    pLayout = new DescriptorSetLayout;
    m_LayoutCache.Add(pLayout, hash);

    VkDescriptorSetLayoutBinding pBindings[LR_MAX_BINDINGS_PER_LAYOUT] = {};
    for (u32 i = 0; i < elements.size(); i++)
    {
        VkDescriptorSetLayoutBinding &binding = pBindings[i];
        DescriptorLayoutElement &element = elements[i];

        binding.binding = element.m_BindingID;
        binding.descriptorType = VKAPI::ToVKDescriptorType(element.m_Type);
        binding.stageFlags = VKAPI::ToVKShaderType(element.m_TargetShader);
        binding.descriptorCount = element.m_ArraySize;
        binding.pImmutableSamplers = nullptr;  // TODO: static samplers
    }

    VkDescriptorSetLayoutCreateInfo layoutCreateInfo = {};
    layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutCreateInfo.bindingCount = elements.size();
    layoutCreateInfo.pBindings = pBindings;

    vkCreateDescriptorSetLayout(m_pDevice, &layoutCreateInfo, nullptr, &pLayout->m_pHandle);

    return pLayout;
}

DescriptorSet *VKAPI::CreateDescriptorSet(DescriptorSetLayout *pLayout)
{
    ZoneScoped;

    DescriptorSet *pSet = new DescriptorSet;
    pSet->m_pBoundLayout = pLayout;

    VkDescriptorSetAllocateInfo allocateCreateInfo = {};
    allocateCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocateCreateInfo.descriptorPool = m_pDescriptorPool;
    allocateCreateInfo.descriptorSetCount = 1;
    allocateCreateInfo.pSetLayouts = &pSet->m_pBoundLayout->m_pHandle;

    vkAllocateDescriptorSets(m_pDevice, &allocateCreateInfo, &pSet->m_pHandle);

    return pSet;
}

void VKAPI::DeleteDescriptorSet(DescriptorSet *pSet)
{
    ZoneScoped;

    vkFreeDescriptorSets(m_pDevice, m_pDescriptorPool, 1, &pSet->m_pHandle);

    delete pSet;
}

void VKAPI::UpdateDescriptorSet(DescriptorSet *pSet, cinitl<DescriptorWriteData> &elements)
{
    ZoneScoped;

    DescriptorSetLayout *pLayout = pSet->m_pBoundLayout;

    VkWriteDescriptorSet pWriteSets[LR_MAX_BINDINGS_PER_LAYOUT] = {};
    VkDescriptorBufferInfo pBufferInfos[LR_MAX_BINDINGS_PER_LAYOUT] = {};
    VkDescriptorImageInfo pImageInfos[LR_MAX_BINDINGS_PER_LAYOUT * 2] = {};  // *2 for samplers

    u32 bufferIndex = 0;
    u32 imageIndex = 0;
    u32 descriptorCount = 0;

    for (const DescriptorWriteData &element : elements)
    {
        VkWriteDescriptorSet &writeSet = pWriteSets[descriptorCount++];
        writeSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeSet.pNext = nullptr;
        writeSet.dstBinding = element.m_Binding;
        writeSet.dstSet = pSet->m_pHandle;
        writeSet.dstArrayElement = 0;
        writeSet.descriptorCount = element.m_Count;
        writeSet.descriptorType = ToVKDescriptorType(element.m_Type);

        switch (element.m_Type)
        {
            case LR_DESCRIPTOR_TYPE_CONSTANT_BUFFER:
            case LR_DESCRIPTOR_TYPE_SHADER_RESOURCE_BUFFER:
            case LR_DESCRIPTOR_TYPE_UNORDERED_ACCESS_BUFFER:
            {
                VkDescriptorBufferInfo &bufferInfo = pBufferInfos[bufferIndex++];
                bufferInfo.buffer = element.m_pBuffer->m_pHandle;
                bufferInfo.offset = element.m_BufferDataOffset;
                bufferInfo.range = element.m_BufferDataSize;

                writeSet.pBufferInfo = &bufferInfo;
                break;
            }

            case LR_DESCRIPTOR_TYPE_SHADER_RESOURCE_IMAGE:
            case LR_DESCRIPTOR_TYPE_UNORDERED_ACCESS_IMAGE:
            {
                VkDescriptorImageInfo &imageInfo = pImageInfos[imageIndex++];
                imageInfo.imageView = element.m_pImage->m_pViewHandle;
                imageInfo.imageLayout = ToVKImageLayout(element.m_ImageLayout);

                writeSet.pImageInfo = &imageInfo;
                break;
            }

            case LR_DESCRIPTOR_TYPE_SAMPLER:
            {
                VkDescriptorImageInfo &imageInfo = pImageInfos[imageIndex++];
                imageInfo.sampler = element.m_pSampler->m_pHandle;

                writeSet.pImageInfo = &imageInfo;
                break;
            }

            default:
                break;
        }
    }

    vkUpdateDescriptorSets(m_pDevice, descriptorCount, pWriteSets, 0, nullptr);
}

VkDescriptorPool VKAPI::CreateDescriptorPool(const std::initializer_list<DescriptorPoolDesc> &layouts)
{
    ZoneScoped;

    VkDescriptorPool pHandle = nullptr;
    VkDescriptorPoolSize pPoolSizes[LR_DESCRIPTOR_TYPE_COUNT];

    u32 maxSets = 0;
    u32 idx = 0;
    for (auto &element : layouts)
    {
        pPoolSizes[idx].type = ToVKDescriptorType(element.m_Type);
        pPoolSizes[idx].descriptorCount = element.m_Count;
        maxSets += element.m_Count;

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

VkDeviceMemory VKAPI::CreateHeap(u64 heapSize, bool cpuWrite)
{
    ZoneScoped;

    VkMemoryPropertyFlags memProps =
        cpuWrite ? VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
                 : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    VkMemoryAllocateInfo memoryAllocInfo = {};
    memoryAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryAllocInfo.allocationSize = heapSize;
    memoryAllocInfo.memoryTypeIndex = GetMemoryTypeIndex(memProps);

    VkDeviceMemory pHandle = nullptr;
    vkAllocateMemory(m_pDevice, &memoryAllocInfo, nullptr, &pHandle);

    return pHandle;
}

Buffer *VKAPI::CreateBuffer(BufferDesc *pDesc)
{
    ZoneScoped;

    Buffer *pBuffer = new Buffer;
    pBuffer->Init(pDesc);

    /// ---------------------------------------------- ///

    VkBufferCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    createInfo.size = pBuffer->m_DataLen;
    createInfo.usage = ToVKBufferUsage(pBuffer->m_UsageFlags);

    vkCreateBuffer(m_pDevice, &createInfo, nullptr, &pBuffer->m_pHandle);

    /// ---------------------------------------------- ///

    SetAllocator(pBuffer, pBuffer->m_TargetAllocator);

    vkBindBufferMemory(m_pDevice, pBuffer->m_pHandle, pBuffer->m_pMemoryHandle, pBuffer->m_DeviceDataOffset);

    return pBuffer;
}

void VKAPI::DeleteBuffer(Buffer *pBuffer, bool waitForWork)
{
    ZoneScoped;

    if (waitForWork)
    {
        SwapChainFrame *pCurrentFrame = GetCurrentFrame();
        pCurrentFrame->m_BufferDeleteQueue.push(pBuffer);
        return;
    }

    if (pBuffer->m_pMemoryHandle)
    {
        if (pBuffer->m_TargetAllocator == LR_API_ALLOCATOR_BUFFER_TLSF)
        {
            Memory::TLSFBlock *pBlock = (Memory::TLSFBlock *)pBuffer->m_pAllocatorData;
            assert(pBlock != nullptr);

            m_MABufferTLSF.Allocator.Free(pBlock);
        }
        else if (pBuffer->m_TargetAllocator == LR_API_ALLOCATOR_NONE)
        {
            vkFreeMemory(m_pDevice, pBuffer->m_pMemoryHandle, nullptr);
        }
    }

    if (pBuffer->m_pHandle)
        vkDestroyBuffer(m_pDevice, pBuffer->m_pHandle, nullptr);
}

void VKAPI::MapMemory(Buffer *pBuffer, void *&pData)
{
    ZoneScoped;

    vkMapMemory(
        m_pDevice, pBuffer->m_pMemoryHandle, pBuffer->m_DeviceDataOffset, pBuffer->m_DeviceDataLen, 0, &pData);
}

void VKAPI::UnmapMemory(Buffer *pBuffer)
{
    ZoneScoped;

    vkUnmapMemory(m_pDevice, pBuffer->m_pMemoryHandle);
}

Image *VKAPI::CreateImage(ImageDesc *pDesc)
{
    ZoneScoped;

    Image *pImage = new Image;
    pImage->Init(pDesc);

    if (pImage->m_DataLen == 0)
        pImage->m_DataLen = pImage->m_Width * pImage->m_Height * GetImageFormatSize(pImage->m_Format);

    /// ---------------------------------------------- //

    VkImageCreateInfo imageCreateInfo = {};
    imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.format = ToVKFormat(pImage->m_Format);
    imageCreateInfo.usage = ToVKImageUsage(pDesc->m_UsageFlags);
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;

    imageCreateInfo.extent.width = pImage->m_Width;
    imageCreateInfo.extent.height = pImage->m_Height;
    imageCreateInfo.extent.depth = 1;

    imageCreateInfo.initialLayout = ToVKImageLayout(pImage->m_Layout);
    imageCreateInfo.mipLevels = pImage->m_MipMapLevels;
    imageCreateInfo.arrayLayers = pImage->m_ArraySize;
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    vkCreateImage(m_pDevice, &imageCreateInfo, nullptr, &pImage->m_pHandle);

    constexpr ImageUsage kColorUsage =
        LR_IMAGE_USAGE_SHADER_RESOURCE | LR_IMAGE_USAGE_RENDER_TARGET | LR_IMAGE_USAGE_UNORDERED_ACCESS;

    if (pDesc->m_UsageFlags & kColorUsage)
    {
        pImage->m_ImageAspect |= VK_IMAGE_ASPECT_COLOR_BIT;
    }
    else if (pDesc->m_UsageFlags & LR_IMAGE_USAGE_DEPTH_STENCIL)
    {
        pImage->m_ImageAspect |= VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    }

    /// ---------------------------------------------- //

    SetAllocator(pImage, pImage->m_TargetAllocator);

    vkBindImageMemory(m_pDevice, pImage->m_pHandle, pImage->m_pMemoryHandle, pImage->m_DeviceDataOffset);

    CreateImageView(pImage);

    return pImage;
}

void VKAPI::DeleteImage(Image *pImage, bool waitForWork)
{
    ZoneScoped;

    if (waitForWork)
    {
        SwapChainFrame *pCurrentFrame = GetCurrentFrame();
        pCurrentFrame->m_ImageDeleteQueue.push(pImage);
        return;
    }

    if (pImage->m_pMemoryHandle)
    {
        if (pImage->m_TargetAllocator == LR_API_ALLOCATOR_IMAGE_TLSF)
        {
            Memory::TLSFBlock *pBlock = (Memory::TLSFBlock *)pImage->m_pAllocatorData;
            assert(pBlock != nullptr);

            m_MAImageTLSF.Allocator.Free(pBlock);
        }
        else if (pImage->m_TargetAllocator == LR_API_ALLOCATOR_NONE)
        {
            vkFreeMemory(m_pDevice, pImage->m_pMemoryHandle, nullptr);
        }
    }

    if (pImage->m_pViewHandle)
        vkDestroyImageView(m_pDevice, pImage->m_pViewHandle, nullptr);

    if (pImage->m_pHandle)
        vkDestroyImage(m_pDevice, pImage->m_pHandle, nullptr);
}

void VKAPI::CreateImageView(Image *pImage)
{
    ZoneScoped;

    VkImageViewCreateInfo imageViewCreateInfo = {};
    imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;

    imageViewCreateInfo.image = pImage->m_pHandle;
    imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    imageViewCreateInfo.format = ToVKFormat(pImage->m_Format);

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

Sampler *VKAPI::CreateSampler(SamplerDesc *pDesc)
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

    samplerInfo.compareEnable = pDesc->m_CompareOp != LR_COMPARE_OP_NEVER;
    samplerInfo.compareOp = (VkCompareOp)pDesc->m_CompareOp;

    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;

    vkCreateSampler(m_pDevice, &samplerInfo, nullptr, &pSampler->m_pHandle);

    return pSampler;
}

void VKAPI::DeleteSampler(VkSampler pSampler)
{
    ZoneScoped;

    vkDestroySampler(m_pDevice, pSampler, nullptr);
}

void VKAPI::SetAllocator(Buffer *pBuffer, APIAllocatorType targetAllocator)
{
    ZoneScoped;

    VkMemoryRequirements memoryRequirements = {};
    vkGetBufferMemoryRequirements(m_pDevice, pBuffer->m_pHandle, &memoryRequirements);

    pBuffer->m_DeviceDataLen = memoryRequirements.size;

    switch (targetAllocator)
    {
        case LR_API_ALLOCATOR_DESCRIPTOR:
        {
            pBuffer->m_DeviceDataOffset =
                m_MADescriptor.Allocator.Allocate(pBuffer->m_DeviceDataLen, memoryRequirements.alignment);
            pBuffer->m_pMemoryHandle = m_MADescriptor.pHeap;

            break;
        }
        case LR_API_ALLOCATOR_BUFFER_LINEAR:
        {
            pBuffer->m_DeviceDataOffset =
                m_MABufferLinear.Allocator.Allocate(pBuffer->m_DeviceDataLen, memoryRequirements.alignment);
            pBuffer->m_pMemoryHandle = m_MABufferLinear.pHeap;

            break;
        }
        case LR_API_ALLOCATOR_BUFFER_TLSF:
        {
            Memory::TLSFBlock *pBlock =
                m_MABufferTLSF.Allocator.Allocate(pBuffer->m_DeviceDataLen, memoryRequirements.alignment);

            pBuffer->m_pAllocatorData = pBlock;
            pBuffer->m_DeviceDataOffset = pBlock->m_Offset;
            pBuffer->m_pMemoryHandle = m_MABufferTLSF.pHeap;

            break;
        }
        case LR_API_ALLOCATOR_BUFFER_TLSF_HOST:
        {
            Memory::TLSFBlock *pBlock =
                m_MABufferTLSFHost.Allocator.Allocate(pBuffer->m_DeviceDataLen, memoryRequirements.alignment);

            pBuffer->m_pAllocatorData = pBlock;
            pBuffer->m_DeviceDataOffset = pBlock->m_Offset;
            pBuffer->m_pMemoryHandle = m_MABufferTLSFHost.pHeap;

            break;
        }
        case LR_API_ALLOCATOR_BUFFER_FRAMETIME:
        {
            u32 currentFrame = m_SwapChain.m_CurrentFrame;
            pBuffer->m_DeviceDataOffset = m_MABufferFrametime[currentFrame].Allocator.Allocate(
                pBuffer->m_DeviceDataLen, memoryRequirements.alignment);
            pBuffer->m_pMemoryHandle = m_MABufferFrametime[currentFrame].pHeap;

            break;
        }
        default:
            LOG_CRITICAL("Trying to allocate VKBuffer resource from invalid allocator.");
            break;
    }
}

void VKAPI::SetAllocator(Image *pImage, APIAllocatorType targetAllocator)
{
    ZoneScoped;

    VkMemoryRequirements memoryRequirements = {};
    vkGetImageMemoryRequirements(m_pDevice, pImage->m_pHandle, &memoryRequirements);

    pImage->m_DeviceDataLen = memoryRequirements.size;

    switch (targetAllocator)
    {
        case LR_API_ALLOCATOR_NONE:
        {
            pImage->m_pMemoryHandle =
                CreateHeap(Memory::AlignUp(memoryRequirements.size, memoryRequirements.alignment), false);

            break;
        }
        case LR_API_ALLOCATOR_IMAGE_TLSF:
        {
            Memory::TLSFBlock *pBlock =
                m_MAImageTLSF.Allocator.Allocate(pImage->m_DataLen, memoryRequirements.alignment);

            pImage->m_pAllocatorData = pBlock;
            pImage->m_DeviceDataOffset = pBlock->m_Offset;
            pImage->m_pMemoryHandle = m_MAImageTLSF.pHeap;

            break;
        }
        default:
            break;
    }
}

ImageFormat &VKAPI::GetSwapChainImageFormat()
{
    return m_SwapChain.m_ImageFormat;
}

Semaphore *VKAPI::GetAvailableAcquireSemaphore(Semaphore *pOldSemaphore)
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

bool VKAPI::IsFormatSupported(ImageFormat format, VkColorSpaceKHR *pColorSpaceOut)
{
    ZoneScoped;

    VkFormat vkFormat = ToVKFormat(format);

    for (u32 i = 0; i < m_ValidSurfaceFormatCount; i++)
    {
        VkSurfaceFormatKHR &surfFormat = m_pValidSurfaceFormats[i];

        if (surfFormat.format == vkFormat)
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

u32 VKAPI::GetMemoryTypeIndex(u32 setBits, VkMemoryPropertyFlags propFlags)
{
    ZoneScoped;

    for (u32 i = 0; i < m_MemoryProps.memoryTypeCount; i++)
    {
        if ((setBits >> i) && ((m_MemoryProps.memoryTypes[i].propertyFlags & propFlags) == propFlags))
            return i;
    }

    LOG_ERROR("Memory type index is not found. Bits: {}.", setBits);

    return -1;
}

u32 VKAPI::GetMemoryTypeIndex(VkMemoryPropertyFlags propFlags)
{
    ZoneScoped;

    for (u32 i = 0; i < m_MemoryProps.memoryTypeCount; i++)
    {
        if ((m_MemoryProps.memoryTypes[i].propertyFlags & propFlags) == propFlags)
            return i;
    }

    LOG_ERROR("Memory type index is not found.");

    return -1;
}

bool VKAPI::LoadVulkan()
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
    }                                                              \
    // LOG_TRACE("Loading Vulkan function: {}:{}", #_name, (void *)_name);

    _VK_IMPORT_SYMBOLS
#undef _VK_DEFINE_FUNCTION

    return true;
}

bool VKAPI::SetupInstance(BaseWindow *pWindow)
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

    /// Initialize surface

    Win32Window *pOSWindow = (Win32Window *)pWindow;
    VkWin32SurfaceCreateInfoKHR surfaceInfo = {};
    surfaceInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    surfaceInfo.pNext = nullptr;
    surfaceInfo.hwnd = (HWND)pOSWindow->m_pHandle;
    surfaceInfo.hinstance = pOSWindow->m_Instance;

    VKCallRet(vkCreateInstance(&createInfo, nullptr, &m_pInstance), "Failed to create Vulkan Instance.", false);
    VKCallRet(
        vkCreateWin32SurfaceKHR(m_pInstance, &surfaceInfo, nullptr, &m_pSurface),
        "Failed to create Vulkan Win32 surface.",
        false);

    /// LOAD INSTANCE EXCLUSIVE FUNCTIONS ///

#define _VK_DEFINE_FUNCTION(_name)                                          \
    _name = (PFN_##_name)vkGetInstanceProcAddr(m_pInstance, #_name);        \
    if (_name == nullptr)                                                   \
    {                                                                       \
        LOG_CRITICAL("Cannot load Vulkan Instance function '{}'!", #_name); \
    }

    _VK_IMPORT_INSTANCE_SYMBOLS
#undef _VK_DEFINE_FUNCTION

    return true;
}

bool VKAPI::SetupQueues(VkPhysicalDevice &pPhysicalDevice, VkPhysicalDeviceFeatures &features)
{
    ZoneScoped;

    u32 queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(pPhysicalDevice, &queueFamilyCount, nullptr);

    VkQueueFamilyProperties *pQueueFamilyProps = new VkQueueFamilyProperties[queueFamilyCount];
    vkGetPhysicalDeviceQueueFamilyProperties(pPhysicalDevice, &queueFamilyCount, pQueueFamilyProps);

    LOG_INFO("Checking available queues({} number of queues available):", queueFamilyCount);

    u32 foundGraphicsFamilyIndex = -1;
    u32 foundComputeFamilyIndex = -1;
    u32 foundTransferFamilyIndex = -1;

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

            LOG_INFO("\tQueue{} selected as Graphics/Present queue.", i);
            m_QueueInfo.m_QueueCount++;
            foundGraphicsFamilyIndex = i;

            continue;
        }

        constexpr u32 kComputeFilter = VK_QUEUE_GRAPHICS_BIT;
        if (foundComputeFamilyIndex == -1 && props.queueFlags & VK_QUEUE_COMPUTE_BIT
            && (props.queueFlags & kComputeFilter) == 0)
        {
            LOG_INFO("\tQueue{} selected as Compute queue.", i);
            m_QueueInfo.m_QueueCount++;
            foundComputeFamilyIndex = i;

            continue;
        }

        constexpr u32 kTransferFilter = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;
        if (foundTransferFamilyIndex == -1 && props.queueFlags & VK_QUEUE_TRANSFER_BIT
            && (props.queueFlags & kTransferFilter) == 0)
        {
            LOG_INFO("\tQueue{} selected as Transfer queue.", i);
            m_QueueInfo.m_QueueCount++;
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

    m_QueueInfo.m_Indexes[LR_COMMAND_LIST_TYPE_GRAPHICS] = foundGraphicsFamilyIndex;
    m_QueueInfo.m_Indexes[LR_COMMAND_LIST_TYPE_COMPUTE] = foundComputeFamilyIndex;
    m_QueueInfo.m_Indexes[LR_COMMAND_LIST_TYPE_TRANSFER] = foundTransferFamilyIndex;

    return true;
}

bool VKAPI::SetupDevice(VkPhysicalDevice &pPhysicalDevice, VkPhysicalDeviceFeatures &features)
{
    ZoneScoped;

    constexpr float kHighPrior = 1.0;

    VkDeviceQueueCreateInfo pQueueInfos[3] = {};
    for (u32 i = 0; i < m_QueueInfo.m_QueueCount; i++)
    {
        pQueueInfos[i].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        pQueueInfos[i].queueFamilyIndex = m_QueueInfo.m_Indexes[i];
        pQueueInfos[i].queueCount = 1;
        pQueueInfos[i].pQueuePriorities = &kHighPrior;
    }

    /// Get device extensions

    LOG_INFO("Checking device extensions:");

    u32 deviceExtensionCount = 0;
    vkEnumerateDeviceExtensionProperties(pPhysicalDevice, nullptr, &deviceExtensionCount, nullptr);

    VkExtensionProperties *pDeviceExtensions = new VkExtensionProperties[deviceExtensionCount];
    vkEnumerateDeviceExtensionProperties(pPhysicalDevice, nullptr, &deviceExtensionCount, pDeviceExtensions);

    constexpr eastl::array<const char *, 6> ppRequiredExtensions = {
        "VK_KHR_swapchain",        "VK_KHR_depth_stencil_resolve",   "VK_KHR_dynamic_rendering",
        "VK_KHR_synchronization2", "VK_EXT_extended_dynamic_state2", "VK_KHR_timeline_semaphore",
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

        LOG_INFO("\t{}, {}", pExtensionName, found);

        if (!found)
        {
            LOG_ERROR("Following extension is not found in this device: {}", extensionSV);

            return false;
        }
    }

    ////////////////////////////////////
    /// Device Feature Configuration ///
    ////////////////////////////////////

    VkPhysicalDeviceVulkan11Features vk11Features = {};
    VkPhysicalDeviceVulkan12Features vk12Features = {};
    VkPhysicalDeviceVulkan13Features vk13Features = {};

    // * Vulkan 1.1
    vk11Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
    vk11Features.pNext = &vk12Features;

    // * Vulkan 1.2
    vk12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    vk12Features.pNext = &vk13Features;
    vk12Features.timelineSemaphore = true;

    // * Vulkan 1.3
    vk13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    vk13Features.pNext = nullptr;

    vk13Features.synchronization2 = true;
    vk13Features.dynamicRendering = true;

    VkDeviceCreateInfo deviceCreateInfo = {};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.pNext = &vk11Features;
    deviceCreateInfo.queueCreateInfoCount = m_QueueInfo.m_QueueCount;
    deviceCreateInfo.pQueueCreateInfos = pQueueInfos;
    deviceCreateInfo.enabledExtensionCount = ppRequiredExtensions.count;
    deviceCreateInfo.ppEnabledExtensionNames = ppRequiredExtensions.data();

    if (vkCreateDevice(pPhysicalDevice, &deviceCreateInfo, nullptr, &m_pDevice) != VK_SUCCESS)
    {
        LOG_CRITICAL("Failed to create Vulkan device. ");

        return false;
    }

    return true;
}

/// ---------------- Type Conversion LUTs ---------------- ///

constexpr VkFormat kFormatLUT[] = {
    VK_FORMAT_UNDEFINED,            // Unknown
    VK_FORMAT_R8G8B8A8_UNORM,       // RGBA8F
    VK_FORMAT_R8G8B8A8_SRGB,        // RGBA8_SRGBF
    VK_FORMAT_B8G8R8A8_UNORM,       // BGRA8F
    VK_FORMAT_R16G16B16A16_SFLOAT,  // RGBA16F
    VK_FORMAT_R32G32B32A32_SFLOAT,  // RGBA32F
    VK_FORMAT_R32_UINT,             // R32U
    VK_FORMAT_R32_SFLOAT,           // R32F
    VK_FORMAT_D32_SFLOAT,           // D32F
    VK_FORMAT_D32_SFLOAT_S8_UINT,   // D32FS8U
};

constexpr VkFormat kAttribFormatLUT[] = {
    VK_FORMAT_UNDEFINED,            // LR_VERTEX_ATTRIB_NONE
    VK_FORMAT_R32_SFLOAT,           // LR_VERTEX_ATTRIB_SFLOAT
    VK_FORMAT_R32G32_SFLOAT,        // LR_VERTEX_ATTRIB_SFLOAT2
    VK_FORMAT_R32G32B32_SFLOAT,     // LR_VERTEX_ATTRIB_SFLOAT3
    VK_FORMAT_R32G32B32A32_SFLOAT,  // LR_VERTEX_ATTRIB_SFLOAT4
    VK_FORMAT_R32_UINT,             // LR_VERTEX_ATTRIB_UINT
    VK_FORMAT_R8G8B8A8_UNORM,       // LR_VERTEX_ATTRIB_UINT_4N
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
    VK_CULL_MODE_NONE,       // None
    VK_CULL_MODE_FRONT_BIT,  // Front
    VK_CULL_MODE_BACK_BIT,   // Back
};

constexpr VkDescriptorType kDescriptorTypeLUT[] = {
    VK_DESCRIPTOR_TYPE_SAMPLER,         // LR_DESCRIPTOR_TYPE_SAMPLER
    VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,   // LR_DESCRIPTOR_TYPE_SHADER_RESOURCE_IMAGE
    VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,  // LR_DESCRIPTOR_TYPE_SHADER_RESOURCE_BUFFER
    VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,  // LR_DESCRIPTOR_TYPE_CONSTANT_BUFFER
    VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,   // LR_DESCRIPTOR_TYPE_UNORDERED_ACCESS_IMAGE
    VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,  // LR_DESCRIPTOR_TYPE_UNORDERED_ACCESS_BUFFER
    VK_DESCRIPTOR_TYPE_MAX_ENUM,        // LR_DESCRIPTOR_TYPE_PUSH_CONSTANT
};

constexpr VkImageLayout kImageLayoutLUT[] = {
    VK_IMAGE_LAYOUT_UNDEFINED,             // LR_IMAGE_LAYOUT_UNDEFINED
    VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,       // LR_IMAGE_LAYOUT_PRESENT
    VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,    // LR_IMAGE_LAYOUT_ATTACHMENT
    VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,     // LR_IMAGE_LAYOUT_READ_ONLY
    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,  // LR_IMAGE_LAYOUT_TRANSFER_SRC
    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,  // LR_IMAGE_LAYOUT_TRANSFER_DST
    VK_IMAGE_LAYOUT_GENERAL,               // LR_IMAGE_LAYOUT_UNORDERED_ACCESS
};

VkFormat VKAPI::ToVKFormat(ImageFormat format)
{
    return kFormatLUT[(u32)format];
}

VkFormat VKAPI::ToVKFormat(VertexAttribType format)
{
    return kAttribFormatLUT[(u32)format];
}

VkPrimitiveTopology VKAPI::ToVKTopology(PrimitiveType type)
{
    return kPrimitiveLUT[(u32)type];
}

VkCullModeFlags VKAPI::ToVKCullMode(CullMode mode)
{
    return kCullModeLUT[(u32)mode];
}

VkDescriptorType VKAPI::ToVKDescriptorType(DescriptorType type)
{
    return kDescriptorTypeLUT[(u32)type];
}

VkImageUsageFlags VKAPI::ToVKImageUsage(ImageUsage usage)
{
    u32 v = 0;

    if (usage & LR_IMAGE_USAGE_RENDER_TARGET)
        v |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    if (usage & LR_IMAGE_USAGE_DEPTH_STENCIL)
        v |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

    if (usage & LR_IMAGE_USAGE_SHADER_RESOURCE)
        v |= VK_IMAGE_USAGE_SAMPLED_BIT;

    if (usage & LR_IMAGE_USAGE_TRANSFER_SRC)
        v |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    if (usage & LR_IMAGE_USAGE_TRANSFER_DST)
        v |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    if (usage & LR_IMAGE_USAGE_UNORDERED_ACCESS)
        v |= VK_IMAGE_USAGE_STORAGE_BIT;

    return (VkImageUsageFlags)v;
}

VkBufferUsageFlagBits VKAPI::ToVKBufferUsage(BufferUsage usage)
{
    u32 v = 0;

    if (usage & LR_BUFFER_USAGE_VERTEX_BUFFER)
        v |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

    if (usage & LR_BUFFER_USAGE_INDEX_BUFFER)
        v |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

    if (usage & LR_BUFFER_USAGE_CONSTANT_BUFFER)
        v |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

    if (usage & LR_BUFFER_USAGE_TRANSFER_SRC)
        v |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    if (usage & LR_BUFFER_USAGE_TRANSFER_DST)
        v |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    if (usage & LR_BUFFER_USAGE_UNORDERED_ACCESS)
        v |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

    return (VkBufferUsageFlagBits)v;
}

VkImageLayout VKAPI::ToVKImageLayout(ImageLayout layout)
{
    return kImageLayoutLUT[layout];
}

VkShaderStageFlagBits VKAPI::ToVKShaderType(ShaderStage type)
{
    u32 v = 0;

    if (type & LR_SHADER_STAGE_VERTEX)
        v |= VK_SHADER_STAGE_VERTEX_BIT;

    if (type & LR_SHADER_STAGE_PIXEL)
        v |= VK_SHADER_STAGE_FRAGMENT_BIT;

    if (type & LR_SHADER_STAGE_COMPUTE)
        v |= VK_SHADER_STAGE_COMPUTE_BIT;

    if (type & LR_SHADER_STAGE_HULL)
        v |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;

    if (type & LR_SHADER_STAGE_DOMAIN)
        v |= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;

    return (VkShaderStageFlagBits)v;
}

VkPipelineStageFlags2 VKAPI::ToVKPipelineStage(PipelineStage stage)
{
    u32 v = 0;

    if (stage == LR_PIPELINE_STAGE_NONE)
        return VK_PIPELINE_STAGE_2_NONE;

    if (stage & LR_PIPELINE_STAGE_VERTEX_INPUT)
        v |= VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT;

    if (stage & LR_PIPELINE_STAGE_VERTEX_SHADER)
        v |= VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;

    if (stage & LR_PIPELINE_STAGE_PIXEL_SHADER)
        v |= VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;

    if (stage & LR_PIPELINE_STAGE_EARLY_PIXEL_TESTS)
        v |= VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT;

    if (stage & LR_PIPELINE_STAGE_LATE_PIXEL_TESTS)
        v |= VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;

    if (stage & LR_PIPELINE_STAGE_RENDER_TARGET)
        v |= VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

    if (stage & LR_PIPELINE_STAGE_COMPUTE_SHADER)
        v |= VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;

    if (stage & LR_PIPELINE_STAGE_TRANSFER)
        v |= VK_PIPELINE_STAGE_2_TRANSFER_BIT;

    if (stage & LR_PIPELINE_STAGE_ALL_COMMANDS)
        v |= VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;

    if (stage & LR_PIPELINE_STAGE_HOST)
        v |= VK_PIPELINE_STAGE_2_HOST_BIT;

    return (VkPipelineStageFlagBits)v;
}

VkAccessFlags2 VKAPI::ToVKAccessFlags(MemoryAcces access)
{
    u32 v = 0;

    if (access == LR_MEMORY_ACCESS_NONE)
        return VK_ACCESS_2_NONE;

    if (access & LR_MEMORY_ACCESS_VERTEX_ATTRIB_READ)
        v |= VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT;

    if (access & LR_MEMORY_ACCESS_INDEX_ATTRIB_READ)
        v |= VK_ACCESS_2_INDEX_READ_BIT;

    if (access & LR_MEMORY_ACCESS_SHADER_READ)
        v |= VK_ACCESS_2_SHADER_READ_BIT;

    if (access & LR_MEMORY_ACCESS_SHADER_WRITE)
        v |= VK_ACCESS_2_SHADER_WRITE_BIT;

    if (access & LR_MEMORY_ACCESS_RENDER_TARGET_READ)
        v |= VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT;

    if (access & LR_MEMORY_ACCESS_RENDER_TARGET_WRITE)
        v |= VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;

    if (access & LR_MEMORY_ACCESS_DEPTH_STENCIL_READ)
        v |= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;

    if (access & LR_MEMORY_ACCESS_DEPTH_STENCIL_WRITE)
        v |= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    if (access & LR_MEMORY_ACCESS_TRANSFER_READ)
        v |= VK_ACCESS_2_TRANSFER_READ_BIT;

    if (access & LR_MEMORY_ACCESS_TRANSFER_WRITE)
        v |= VK_ACCESS_2_TRANSFER_WRITE_BIT;

    if (access & LR_MEMORY_ACCESS_MEMORY_READ)
        v |= VK_ACCESS_2_MEMORY_READ_BIT;

    if (access & LR_MEMORY_ACCESS_MEMORY_WRITE)
        v |= VK_ACCESS_2_MEMORY_WRITE_BIT;

    if (access & LR_MEMORY_ACCESS_HOST_READ)
        v |= VK_ACCESS_2_HOST_READ_BIT;

    if (access & LR_MEMORY_ACCESS_HOST_WRITE)
        v |= VK_ACCESS_2_HOST_WRITE_BIT;

    return (VkAccessFlags2)v;
}

}  // namespace lr::Graphics