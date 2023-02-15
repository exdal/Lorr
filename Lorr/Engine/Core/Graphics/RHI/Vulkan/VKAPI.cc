#include "VKAPI.hh"

#include "Engine.hh"

#include "Core/Crypto/Light.hh"

#include "VKCommandList.hh"
#include "VKShader.hh"

/// DEFINE VULKAN FUNCTIONS
// #include "VKSymbols.hh"
#define _VK_DEFINE_FUNCTION(_name) PFN_##_name _name
// Define symbols
_VK_IMPORT_SYMBOLS
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

// Example: `BaseCommandQueue *pQueue` -> `VKCommandQueue *pQueueVK`
#define API_VAR(type, name) type *name##VK = (type *)name

namespace lr::Graphics
{
    bool VKAPI::Init(BaseAPIDesc *pDesc)
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
        vkGetPhysicalDeviceSurfaceFormatsKHR(
            m_pPhysicalDevice, m_pSurface, &m_ValidSurfaceFormatCount, nullptr);

        m_pValidSurfaceFormats = new VkSurfaceFormatKHR[m_ValidSurfaceFormatCount];
        vkGetPhysicalDeviceSurfaceFormatsKHR(
            m_pPhysicalDevice, m_pSurface, &m_ValidSurfaceFormatCount, m_pValidSurfaceFormats);

        /// Get surface present modes
        vkGetPhysicalDeviceSurfacePresentModesKHR(
            m_pPhysicalDevice, m_pSurface, &m_ValidPresentModeCount, nullptr);

        m_pValidPresentModes = new VkPresentModeKHR[m_ValidPresentModeCount];
        vkGetPhysicalDeviceSurfacePresentModesKHR(
            m_pPhysicalDevice, m_pSurface, &m_ValidPresentModeCount, m_pValidPresentModes);

        vkGetPhysicalDeviceMemoryProperties(m_pPhysicalDevice, &m_MemoryProps);

        // yeah uh, dont delete
        // delete[] ppAvailableDevices;

        InitAllocators(&pDesc->m_AllocatorDesc);

        m_CommandListPool.Init();
        m_pDescriptorPool = CreateDescriptorPool({
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 256 },
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 256 },
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 256 },
        });

        m_pPipelineCache = CreatePipelineCache();

        m_pDirectQueue = (VKCommandQueue *)CreateCommandQueue(CommandListType::Direct);
        m_pComputeQueue = (VKCommandQueue *)CreateCommandQueue(CommandListType::Compute);

        m_SwapChain.Init(pDesc->m_pTargetWindow, this, pDesc->m_SwapChainFlags);
        m_SwapChain.CreateBackBuffers(this);

        LOG_TRACE("Successfully initialized Vulkan context.");

        for (CommandList *&pList : m_CommandListPool.m_DirectLists)
            pList = CreateCommandList(CommandListType::Direct);

        for (CommandList *&pList : m_CommandListPool.m_ComputeLists)
            pList = CreateCommandList(CommandListType::Compute);

        for (CommandList *&pList : m_CommandListPool.m_CopyLists)
            pList = CreateCommandList(CommandListType::Copy);

        m_FenceThread.Begin(VKAPI::TFFenceWait, this);

        LOG_TRACE("Initialized Vulkan render context.");

        return true;
    }

    void VKAPI::InitAllocators(APIAllocatorInitDesc *pDesc)
    {
        ZoneScoped;

        constexpr u32 kTypeMem = Memory::MiBToBytes(16);

        m_TypeAllocator.Init(kTypeMem, 0x2000);
        m_pTypeData = Memory::Allocate<u8>(kTypeMem);

        m_MADescriptor.Allocator.Init(pDesc->m_DescriptorMem);
        m_MADescriptor.pHeap = CreateHeap(pDesc->m_DescriptorMem, true);

        m_MABufferLinear.Allocator.Init(pDesc->m_BufferLinearMem);
        m_MABufferLinear.pHeap = CreateHeap(pDesc->m_BufferLinearMem, true);

        m_MABufferTLSF.Allocator.Init(pDesc->m_BufferTLSFMem, pDesc->m_MaxTLSFAllocations);
        m_MABufferTLSF.pHeap = CreateHeap(pDesc->m_BufferTLSFMem, false);

        m_MABufferTLSFHost.Allocator.Init(pDesc->m_BufferTLSFHostMem, pDesc->m_MaxTLSFAllocations);
        m_MABufferTLSFHost.pHeap = CreateHeap(pDesc->m_BufferTLSFHostMem, true);

        m_MABufferFrametime.Allocator.Init(pDesc->m_BufferFrametimeMem);
        m_MABufferFrametime.pHeap = CreateHeap(pDesc->m_BufferFrametimeMem, true);

        m_MAImageTLSF.Allocator.Init(pDesc->m_ImageTLSFMem, pDesc->m_MaxTLSFAllocations);
        m_MAImageTLSF.pHeap = CreateHeap(pDesc->m_ImageTLSFMem, false);
    }

    VKCommandQueue *VKAPI::CreateCommandQueue(CommandListType type)
    {
        ZoneScoped;

        VKCommandQueue *pQueue = AllocType<VKCommandQueue>();

        VkQueue pHandle = nullptr;
        vkGetDeviceQueue(m_pDevice, m_QueueIndexes[(u32)type], 0, &pHandle);

        pQueue->Init(pHandle);

        return pQueue;
    }

    VKCommandAllocator *VKAPI::CreateCommandAllocator(CommandListType type)
    {
        ZoneScoped;

        VKCommandAllocator *pAllocator = AllocTypeInherit<CommandAllocator, VKCommandAllocator>();

        VkCommandPoolCreateInfo allocatorInfo = {};
        allocatorInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        allocatorInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        allocatorInfo.queueFamilyIndex = m_QueueIndexes[(u32)type];

        vkCreateCommandPool(m_pDevice, &allocatorInfo, nullptr, &pAllocator->m_pHandle);

        return pAllocator;
    }

    VKCommandList *VKAPI::CreateCommandList(CommandListType type)
    {
        ZoneScoped;

        VKCommandAllocator *pAllocator = CreateCommandAllocator(type);
        VKCommandList *pList = AllocTypeInherit<CommandList, VKCommandList>();

        VkCommandBuffer pHandle = nullptr;
        VkFence pFence = CreateFence(false);

        VkCommandBufferAllocateInfo listInfo = {};
        listInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        listInfo.commandBufferCount = 1;
        listInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        listInfo.commandPool = pAllocator->m_pHandle;

        vkAllocateCommandBuffers(m_pDevice, &listInfo, &pHandle);

        pList->Init(pHandle, pFence, type);

        return pList;
    }

    void VKAPI::BeginCommandList(CommandList *pList)
    {
        ZoneScoped;

        API_VAR(VKCommandList, pList);

        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.pNext = nullptr;
        beginInfo.flags = 0;

        vkBeginCommandBuffer(pListVK->m_pHandle, &beginInfo);
    }

    void VKAPI::EndCommandList(CommandList *pList)
    {
        ZoneScoped;

        API_VAR(VKCommandList, pList);

        vkEndCommandBuffer(pListVK->m_pHandle);
    }

    void VKAPI::ResetCommandAllocator(CommandAllocator *pAllocator)
    {
        ZoneScoped;

        API_VAR(VKCommandAllocator, pAllocator);

        vkResetCommandPool(m_pDevice, pAllocatorVK->m_pHandle, 0);
    }

    void VKAPI::ExecuteCommandList(CommandList *pList, bool waitForFence)
    {
        ZoneScoped;

        API_VAR(VKCommandList, pList);

        VkQueue pQueueHandle = nullptr;
        VkFence &pFence = pListVK->m_pFence;

        switch (pList->m_Type)
        {
            case CommandListType::Direct:
                pQueueHandle = m_pDirectQueue->m_pHandle;
                break;
            case CommandListType::Compute:
                pQueueHandle = m_pComputeQueue->m_pHandle;
                break;
            case CommandListType::Copy:
                pQueueHandle = m_pTransferQueue->m_pHandle;
                break;
            default:
                break;
        }

        VkCommandBufferSubmitInfo commandBufferInfo = {};
        commandBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
        commandBufferInfo.commandBuffer = pListVK->m_pHandle;

        VkSubmitInfo2 submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
        submitInfo.commandBufferInfoCount = 1;
        submitInfo.pCommandBufferInfos = &commandBufferInfo;

        vkQueueSubmit2(pQueueHandle, 1, &submitInfo, pFence);

        if (waitForFence)
        {
            vkWaitForFences(m_pDevice, 1, &pFence, false, UINT64_MAX);
            vkResetFences(m_pDevice, 1, &pFence);

            m_CommandListPool.Release(pListVK);
        }
        else
        {
            m_CommandListPool.SignalFence(pListVK);
        }
    }

    void VKAPI::PresentCommandQueue()
    {
        ZoneScoped;

        VkQueue &pQueueHandle = m_pDirectQueue->m_pHandle;

        VKSwapChainFrame *pCurrentFrame = m_SwapChain.GetCurrentFrame();

        VkSemaphore &pAcquireSemp = pCurrentFrame->m_pAcquireSemp;
        VkSemaphore &pPresentSemp = pCurrentFrame->m_pPresentSemp;

        VkSemaphoreSubmitInfo acquireSemaphoreInfo = {};
        acquireSemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        acquireSemaphoreInfo.semaphore = pAcquireSemp;
        acquireSemaphoreInfo.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR;

        VkSemaphoreSubmitInfo presentSemaphoreInfo = {};
        presentSemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        presentSemaphoreInfo.semaphore = pPresentSemp;
        presentSemaphoreInfo.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;

        VkSubmitInfo2 submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
        submitInfo.waitSemaphoreInfoCount = 1;
        submitInfo.pWaitSemaphoreInfos = &acquireSemaphoreInfo;
        submitInfo.signalSemaphoreInfoCount = 1;
        submitInfo.pSignalSemaphoreInfos = &presentSemaphoreInfo;

        vkQueueSubmit2(pQueueHandle, 1, &submitInfo, nullptr);
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

    void VKAPI::DeleteFence(VkFence pFence)
    {
        ZoneScoped;

        vkDestroyFence(m_pDevice, pFence, nullptr);
    }

    bool VKAPI::IsFenceSignaled(VkFence pFence)
    {
        ZoneScoped;

        return vkGetFenceStatus(m_pDevice, pFence) == VK_SUCCESS;
    }

    void VKAPI::ResetFence(VkFence pFence)
    {
        ZoneScoped;

        vkResetFences(m_pDevice, 1, &pFence);
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

    void VKAPI::DeleteSemaphore(VkSemaphore pSemaphore)
    {
        ZoneScoped;

        vkDestroySemaphore(m_pDevice, pSemaphore, nullptr);
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

        VkDescriptorSetLayout pSetLayouts[LR_MAX_DESCRIPTOR_SETS_PER_PIPELINE + 1] = {};

        u32 currentSet = 0;
        for (u32 i = 0; i < pDesc->m_DescriptorSetCount; i++)
        {
            DescriptorSet *pSet = pDesc->m_ppDescriptorSets[i];
            API_VAR(VKDescriptorSet, pSet);

            pSetLayouts[currentSet++] = pSetVK->m_pSetLayoutHandle;
        }

        VkPushConstantRange pPushConstants[LR_MAX_PUSH_CONSTANTS_PER_PIPELINE] = {};

        for (u32 i = 0; i < pDesc->m_PushConstantCount; i++)
        {
            PushConstantDesc &pushConstant = pDesc->m_pPushConstants[i];

            pPushConstants[i].stageFlags = ToVKShaderType(pushConstant.m_Stage);
            pPushConstants[i].offset = pushConstant.m_Offset;
            pPushConstants[i].size = pushConstant.m_Size;
        }

        VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
        pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutCreateInfo.setLayoutCount = currentSet;
        pipelineLayoutCreateInfo.pSetLayouts = pSetLayouts;
        pipelineLayoutCreateInfo.pushConstantRangeCount = pDesc->m_PushConstantCount;
        pipelineLayoutCreateInfo.pPushConstantRanges = pPushConstants;

        VkPipelineLayout pLayout = nullptr;
        vkCreatePipelineLayout(m_pDevice, &pipelineLayoutCreateInfo, nullptr, &pLayout);

        return pLayout;
    }

    Pipeline *VKAPI::CreateGraphicsPipeline(GraphicsPipelineBuildInfo *pBuildInfo)
    {
        ZoneScoped;

        VKPipeline *pPipeline = AllocTypeInherit<Pipeline, VKPipeline>();

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

        PipelineLayoutSerializeDesc layoutDesc = {};
        layoutDesc.m_DescriptorSetCount = pBuildInfo->m_DescriptorSetCount;
        layoutDesc.m_ppDescriptorSets = pBuildInfo->m_ppDescriptorSets;
        layoutDesc.m_PushConstantCount = pBuildInfo->m_PushConstantCount;
        layoutDesc.m_pPushConstants = pBuildInfo->m_pPushConstants;

        VkPipelineLayout pLayout = SerializePipelineLayout(&layoutDesc, pPipeline);
        pPipeline->m_pLayout = pLayout;

        /// BOUND RENDER TARGETS -----------------------------------------------------

        VkFormat pRenderTargetFormats[LR_MAX_RENDER_TARGET_PER_PASS] = {};

        VkPipelineRenderingCreateInfo renderingInfo = {};
        renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
        renderingInfo.pNext = nullptr;
        renderingInfo.viewMask = 0;
        renderingInfo.colorAttachmentCount = pBuildInfo->m_RenderTargetCount;
        renderingInfo.pColorAttachmentFormats = pRenderTargetFormats;
        renderingInfo.depthAttachmentFormat = ToVKFormat(pBuildInfo->m_DepthAttachment.m_Format);

        for (u32 i = 0; i < pBuildInfo->m_RenderTargetCount; i++)
        {
            pRenderTargetFormats[i] = ToVKFormat(pBuildInfo->m_pRenderTargets[i].m_Format);
        }

        /// BOUND SHADER STAGES ------------------------------------------------------

        VkPipelineShaderStageCreateInfo pShaderStageInfos[LR_SHADER_STAGE_COUNT] = {};
        for (u32 i = 0; i < pBuildInfo->m_ShaderCount; i++)
        {
            Shader *pShader = pBuildInfo->m_ppShaders[i];
            API_VAR(VKShader, pShader);

            VkPipelineShaderStageCreateInfo &info = pShaderStageInfos[i];
            info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            info.pName = nullptr;
            info.stage = ToVKShaderType(pShaderVK->m_Type);
            info.module = pShaderVK->m_pHandle;
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
        colorBlendInfo.attachmentCount = pBuildInfo->m_RenderTargetCount;
        colorBlendInfo.pAttachments = pBlendAttachmentInfos;

        for (u32 i = 0; i < pBuildInfo->m_RenderTargetCount; i++)
        {
            PipelineAttachment &attachment = pBuildInfo->m_pRenderTargets[i];
            VkPipelineColorBlendAttachmentState &renderTarget = pBlendAttachmentInfos[i];

            renderTarget.blendEnable = attachment.m_BlendEnable;
            renderTarget.colorWriteMask = attachment.m_WriteMask;
            renderTarget.srcColorBlendFactor = kBlendFactorLUT[(u32)attachment.m_SrcBlend];
            renderTarget.dstColorBlendFactor = kBlendFactorLUT[(u32)attachment.m_DstBlend];
            renderTarget.srcAlphaBlendFactor = kBlendFactorLUT[(u32)attachment.m_SrcBlendAlpha];
            renderTarget.dstAlphaBlendFactor = kBlendFactorLUT[(u32)attachment.m_DstBlendAlpha];
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
        createInfo.stageCount = pBuildInfo->m_ShaderCount;
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

        VKPipeline *pPipeline = AllocTypeInherit<Pipeline, VKPipeline>();

        PipelineLayoutSerializeDesc layoutDesc = {};
        layoutDesc.m_DescriptorSetCount = pBuildInfo->m_DescriptorSetCount;
        layoutDesc.m_ppDescriptorSets = pBuildInfo->m_ppDescriptorSets;
        layoutDesc.m_PushConstantCount = pBuildInfo->m_PushConstantCount;
        layoutDesc.m_pPushConstants = pBuildInfo->m_pPushConstants;

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
        shaderStage.module = ((VKShader *)pBuildInfo->m_pShader)->m_pHandle;
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

    void VKAPI::DeleteSwapChain(VKSwapChain *pSwapChain)
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

        WaitForDevice();

        m_SwapChain.m_Width = width;
        m_SwapChain.m_Height = height;

        m_SwapChain.DestroyHandle(this);
        m_SwapChain.CreateHandle(this);
        m_SwapChain.CreateBackBuffers(this);
    }

    BaseSwapChain *VKAPI::GetSwapChain()
    {
        return &m_SwapChain;
    }

    void VKAPI::BeginFrame()
    {
        ZoneScoped;

        VkSwapchainKHR &pSwapChain = m_SwapChain.m_pHandle;
        VKSwapChainFrame *pCurrentFrame = m_SwapChain.GetCurrentFrame();

        VkSemaphore &pSemaphore = pCurrentFrame->m_pAcquireSemp;

        u32 imageIndex;
        vkAcquireNextImageKHR(m_pDevice, pSwapChain, UINT64_MAX, pSemaphore, nullptr, &imageIndex);

        if (imageIndex != m_SwapChain.m_CurrentFrame)
        {
            LOG_WARN("Current image does not match with SwapChain's current image.");
            m_SwapChain.m_CurrentFrame = imageIndex;
        }
    }

    void VKAPI::EndFrame()
    {
        ZoneScoped;

        VkSwapchainKHR &pSwapChain = m_SwapChain.m_pHandle;
        VkQueue &pQueueHandle = m_pDirectQueue->m_pHandle;
        VKSwapChainFrame *pCurrentFrame = m_SwapChain.GetCurrentFrame();

        PresentCommandQueue();

        VkPresentInfoKHR presentInfo = {};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &pCurrentFrame->m_pPresentSemp;

        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &pSwapChain;

        presentInfo.pImageIndices = &m_SwapChain.m_CurrentFrame;

        vkQueuePresentKHR(pQueueHandle, &presentInfo);

        // There is a possible chance of that image ordering can be wrong
        // We cannot predict the next image index
        m_SwapChain.NextFrame();
        // m_SwapChain.m_CurrentFrame = imageIndex;
    }

    void VKAPI::WaitForDevice()
    {
        ZoneScoped;

        vkDeviceWaitIdle(m_pDevice);
    }

    Shader *VKAPI::CreateShader(ShaderStage stage, BufferReadStream &buf)
    {
        ZoneScoped;

        VKShader *pShader = AllocTypeInherit<Shader, VKShader>();

        pShader->m_Type = stage;

        ShaderCompileDesc compileDesc = {};
        compileDesc.m_Type = stage;
        compileDesc.m_Flags =
            LR_SHADER_FLAG_USE_SPIRV | LR_SHADER_FLAG_MATRIX_ROW_MAJOR | LR_SHADER_FLAG_ALL_RESOURCES_BOUND;

#if 1
        compileDesc.m_Flags |=
            LR_SHADER_FLAG_WARNINGS_ARE_ERRORS | LR_SHADER_FLAG_SKIP_OPTIMIZATION | LR_SHADER_FLAG_USE_DEBUG;
#endif

        IDxcBlob *pCode = ShaderCompiler::CompileFromBuffer(&compileDesc, buf);

        VkShaderModule pHandle = nullptr;

        VkShaderModuleCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;

        createInfo.codeSize = pCode->GetBufferSize();
        createInfo.pCode = (u32 *)pCode->GetBufferPointer();

        vkCreateShaderModule(m_pDevice, &createInfo, nullptr, &pHandle);

        pShader->m_Type = stage;
        pShader->m_pHandle = pHandle;

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

        API_VAR(VKShader, pShader);

        vkDestroyShaderModule(m_pDevice, pShaderVK->m_pHandle, nullptr);

        FreeType(pShaderVK);
    }

    DescriptorSet *VKAPI::CreateDescriptorSet(DescriptorSetDesc *pDesc)
    {
        ZoneScoped;

        VKDescriptorSet *pDescriptorSet = AllocTypeInherit<DescriptorSet, VKDescriptorSet>();
        pDescriptorSet->m_BindingCount = pDesc->m_BindingCount;

        VkDescriptorSetLayoutBinding pBindings[LR_MAX_DESCRIPTORS_PER_LAYOUT] = {};

        for (u32 i = 0; i < pDesc->m_BindingCount; i++)
        {
            DescriptorBindingDesc &element = pDesc->m_pBindings[i];
            VkDescriptorSetLayoutBinding &layoutBinding = pBindings[i];
            pDescriptorSet->m_pBindingTypes[i] = element.m_Type;

            VkDescriptorType type = ToVKDescriptorType(element.m_Type);

            layoutBinding.binding = element.m_BindingID;
            layoutBinding.descriptorType = type;
            layoutBinding.stageFlags = ToVKShaderType(element.m_TargetShader);
            layoutBinding.descriptorCount = element.m_ArraySize;
        }

        // First we need layout info
        VkDescriptorSetLayoutCreateInfo layoutCreateInfo = {};
        layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutCreateInfo.bindingCount = pDesc->m_BindingCount;
        layoutCreateInfo.pBindings = pBindings;

        vkCreateDescriptorSetLayout(m_pDevice, &layoutCreateInfo, nullptr, &pDescriptorSet->m_pSetLayoutHandle);

        // And the actual set
        VkDescriptorSetAllocateInfo allocateCreateInfo = {};
        allocateCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocateCreateInfo.descriptorPool = m_pDescriptorPool;
        allocateCreateInfo.descriptorSetCount = 1;
        allocateCreateInfo.pSetLayouts = &pDescriptorSet->m_pSetLayoutHandle;

        vkAllocateDescriptorSets(m_pDevice, &allocateCreateInfo, &pDescriptorSet->m_pHandle);

        return pDescriptorSet;
    }

    void VKAPI::DeleteDescriptorSet(DescriptorSet *pSet)
    {
        ZoneScoped;

        for (u32 i = 0; i < pSet->m_BindingCount; i++)
        {
        }
    }

    void VKAPI::UpdateDescriptorData(DescriptorSet *pSet, DescriptorSetDesc *pDesc)
    {
        ZoneScoped;

        API_VAR(VKDescriptorSet, pSet);

        VkWriteDescriptorSet pWriteSets[LR_MAX_DESCRIPTORS_PER_LAYOUT] = {};
        VkDescriptorBufferInfo pBufferInfos[LR_MAX_DESCRIPTORS_PER_LAYOUT] = {};
        VkDescriptorImageInfo pImageInfos[LR_MAX_DESCRIPTORS_PER_LAYOUT * 2] = {};  // *2 for samplers

        u32 bufferIndex = 0;
        u32 imageIndex = 0;
        u32 descriptorCount = 0;

        for (u32 i = 0; i < pDesc->m_BindingCount; i++)
        {
            DescriptorBindingDesc &element = pDesc->m_pBindings[i];

            VkWriteDescriptorSet &writeSet = pWriteSets[descriptorCount++];
            VkDescriptorBufferInfo &bufferInfo = pBufferInfos[bufferIndex];
            VkDescriptorImageInfo &imageInfo = pImageInfos[imageIndex];

            Sampler *pSampler = element.m_pSampler;
            Buffer *pBuffer = element.m_pBuffer;
            Image *pImage = element.m_pImage;

            API_VAR(VKSampler, pSampler);
            API_VAR(VKBuffer, pBuffer);
            API_VAR(VKImage, pImage);

            writeSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writeSet.pNext = nullptr;
            writeSet.dstBinding = element.m_BindingID;
            writeSet.dstSet = pSetVK->m_pHandle;
            writeSet.dstArrayElement = 0;
            writeSet.descriptorCount = element.m_ArraySize;
            writeSet.descriptorType = ToVKDescriptorType(element.m_Type);

            switch (element.m_Type)
            {
                case LR_DESCRIPTOR_TYPE_CONSTANT_BUFFER:
                case LR_DESCRIPTOR_TYPE_SHADER_RESOURCE_BUFFER:
                case LR_DESCRIPTOR_TYPE_UNORDERED_ACCESS_BUFFER:
                {
                    bufferInfo.buffer = pBufferVK->m_pHandle;
                    bufferInfo.offset = 0;
                    bufferInfo.range = pBufferVK->m_DataLen;

                    writeSet.pBufferInfo = &bufferInfo;
                    bufferIndex++;
                    break;
                }

                case LR_DESCRIPTOR_TYPE_SHADER_RESOURCE_IMAGE:
                case LR_DESCRIPTOR_TYPE_UNORDERED_ACCESS_IMAGE:
                {
                    imageInfo.imageView = pImageVK->m_pViewHandle;
                    imageInfo.imageLayout = pImageVK->m_Layout;

                    writeSet.pImageInfo = &imageInfo;
                    imageIndex++;
                    break;
                }

                case LR_DESCRIPTOR_TYPE_SAMPLER:
                {
                    imageInfo.sampler = pSamplerVK->m_pHandle;

                    writeSet.pImageInfo = &imageInfo;
                    imageIndex++;
                    break;
                }

                default:
                    break;
            }
        }

        vkUpdateDescriptorSets(m_pDevice, descriptorCount, pWriteSets, 0, nullptr);
    }

    VkDescriptorPool VKAPI::CreateDescriptorPool(const std::initializer_list<VKDescriptorBindingDesc> &layouts)
    {
        ZoneScoped;

        VkDescriptorPool pHandle = nullptr;
        VkDescriptorPoolSize pPoolSizes[LR_DESCRIPTOR_TYPE_COUNT];

        u32 maxSets = 0;
        u32 idx = 0;
        for (auto &element : layouts)
        {
            pPoolSizes[idx].type = element.m_Type;
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

        VKBuffer *pBuffer = AllocTypeInherit<Buffer, VKBuffer>();
        pBuffer->m_UsageFlags = pDesc->m_UsageFlags;
        pBuffer->m_TargetAllocator = pDesc->m_TargetAllocator;
        pBuffer->m_Stride = pDesc->m_Stride;
        pBuffer->m_DataLen = pDesc->m_DataLen;

        /// ---------------------------------------------- ///

        VkBufferCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        createInfo.size = pBuffer->m_DataLen;
        createInfo.usage = ToVKBufferUsage(pBuffer->m_UsageFlags);

        vkCreateBuffer(m_pDevice, &createInfo, nullptr, &pBuffer->m_pHandle);

        /// ---------------------------------------------- ///

        SetAllocator(pBuffer, pBuffer->m_TargetAllocator);

        vkBindBufferMemory(m_pDevice, pBuffer->m_pHandle, pBuffer->m_pMemoryHandle, pBuffer->m_DataOffset);

        return pBuffer;
    }

    void VKAPI::DeleteBuffer(Buffer *pHandle)
    {
        ZoneScoped;

        API_VAR(VKBuffer, pHandle);

        if (pHandleVK->m_pMemoryHandle)
        {
            if (pHandleVK->m_TargetAllocator == LR_API_ALLOCATOR_BUFFER_TLSF)
            {
                Memory::TLSFBlock *pBlock = (Memory::TLSFBlock *)pHandleVK->m_pAllocatorData;
                assert(pBlock != nullptr);

                m_MABufferTLSF.Allocator.Free(pBlock);
            }
            else if (pHandleVK->m_TargetAllocator == LR_API_ALLOCATOR_NONE)
            {
                vkFreeMemory(m_pDevice, pHandleVK->m_pMemoryHandle, nullptr);
            }
        }

        if (pHandleVK->m_pHandle)
            vkDestroyBuffer(m_pDevice, pHandleVK->m_pHandle, nullptr);
    }

    void VKAPI::MapMemory(Buffer *pBuffer, void *&pData)
    {
        ZoneScoped;

        API_VAR(VKBuffer, pBuffer);

        vkMapMemory(
            m_pDevice, pBufferVK->m_pMemoryHandle, pBuffer->m_DataOffset, pBuffer->m_DeviceDataLen, 0, &pData);
    }

    void VKAPI::UnmapMemory(Buffer *pBuffer)
    {
        ZoneScoped;

        API_VAR(VKBuffer, pBuffer);

        vkUnmapMemory(m_pDevice, pBufferVK->m_pMemoryHandle);
    }

    Image *VKAPI::CreateImage(ImageDesc *pDesc)
    {
        ZoneScoped;

        VKImage *pImage = AllocTypeInherit<Image, VKImage>();
        pImage->m_UsageFlags = pDesc->m_UsageFlags;
        pImage->m_Format = pDesc->m_Format;
        pImage->m_TargetAllocator = pDesc->m_TargetAllocator;
        pImage->m_Width = pDesc->m_Width;
        pImage->m_Height = pDesc->m_Height;
        pImage->m_ArraySize = pDesc->m_ArraySize;
        pImage->m_MipMapLevels = pDesc->m_MipMapLevels;
        pImage->m_DataLen = pDesc->m_DataLen;
        pImage->m_DataOffset = 0;
        pImage->m_DeviceDataLen = pDesc->m_DeviceDataLen;
        pImage->m_Layout = VK_IMAGE_LAYOUT_UNDEFINED;

        if (pImage->m_DataLen == 0)
        {
            pImage->m_DataLen = pImage->m_Width * pImage->m_Height * GetImageFormatSize(pImage->m_Format);
        }

        /// ---------------------------------------------- //

        VkImageCreateInfo imageCreateInfo = {};
        imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageCreateInfo.format = ToVKFormat(pImage->m_Format);
        imageCreateInfo.usage = ToVKImageUsage(pImage->m_UsageFlags);
        imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;

        imageCreateInfo.extent.width = pImage->m_Width;
        imageCreateInfo.extent.height = pImage->m_Height;
        imageCreateInfo.extent.depth = 1;

        imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageCreateInfo.mipLevels = pImage->m_MipMapLevels;
        imageCreateInfo.arrayLayers = pImage->m_ArraySize;
        imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        vkCreateImage(m_pDevice, &imageCreateInfo, nullptr, &pImage->m_pHandle);

        /// ---------------------------------------------- //

        SetAllocator(pImage, pImage->m_TargetAllocator);

        vkBindImageMemory(m_pDevice, pImage->m_pHandle, pImage->m_pMemoryHandle, pImage->m_DataOffset);

        CreateImageView(pImage);

        return pImage;
    }

    void VKAPI::DeleteImage(Image *pImage)
    {
        ZoneScoped;

        API_VAR(VKImage, pImage);

        if (pImageVK->m_pMemoryHandle)
        {
            if (pImage->m_TargetAllocator == LR_API_ALLOCATOR_IMAGE_TLSF)
            {
                Memory::TLSFBlock *pBlock = (Memory::TLSFBlock *)pImage->m_pAllocatorData;
                assert(pBlock != nullptr);

                m_MAImageTLSF.Allocator.Free(pBlock);
            }
            else if (pImage->m_TargetAllocator == LR_API_ALLOCATOR_NONE)
            {
                vkFreeMemory(m_pDevice, pImageVK->m_pMemoryHandle, nullptr);
            }
        }

        if (pImageVK->m_pViewHandle)
            vkDestroyImageView(m_pDevice, pImageVK->m_pViewHandle, nullptr);

        if (pImageVK->m_pHandle)
            vkDestroyImage(m_pDevice, pImageVK->m_pHandle, nullptr);
    }

    void VKAPI::CreateImageView(Image *pImage)
    {
        ZoneScoped;

        API_VAR(VKImage, pImage);

        VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_NONE;

        constexpr ResourceUsage kColorUsage = LR_RESOURCE_USAGE_SHADER_RESOURCE
                                              | LR_RESOURCE_USAGE_RENDER_TARGET
                                              | LR_RESOURCE_USAGE_UNORDERED_ACCESS;

        if (pImage->m_UsageFlags & kColorUsage)
        {
            aspectFlags |= VK_IMAGE_ASPECT_COLOR_BIT;
        }

        if (pImage->m_UsageFlags & LR_RESOURCE_USAGE_DEPTH_STENCIL)
        {
            aspectFlags |= VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        }

        VkImageViewCreateInfo imageViewCreateInfo = {};
        imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;

        imageViewCreateInfo.image = pImageVK->m_pHandle;
        imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        imageViewCreateInfo.format = ToVKFormat(pImage->m_Format);

        imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

        imageViewCreateInfo.subresourceRange.aspectMask = aspectFlags;
        imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
        imageViewCreateInfo.subresourceRange.levelCount = pImage->m_MipMapLevels;
        imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
        imageViewCreateInfo.subresourceRange.layerCount = 1;

        vkCreateImageView(m_pDevice, &imageViewCreateInfo, nullptr, &pImageVK->m_pViewHandle);
    }

    Sampler *VKAPI::CreateSampler(SamplerDesc *pDesc)
    {
        ZoneScoped;

        VKSampler *pSampler = AllocTypeInherit<Sampler, VKSampler>();

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

    void VKAPI::SetAllocator(VKBuffer *pBuffer, APIAllocatorType targetAllocator)
    {
        ZoneScoped;

        VkMemoryRequirements memoryRequirements = {};
        vkGetBufferMemoryRequirements(m_pDevice, pBuffer->m_pHandle, &memoryRequirements);

        pBuffer->m_DeviceDataLen = memoryRequirements.size;

        switch (targetAllocator)
        {
            case LR_API_ALLOCATOR_NONE:
            {
                pBuffer->m_DataOffset = 0;
                pBuffer->m_pMemoryHandle = CreateHeap(
                    pBuffer->m_DeviceDataLen, pBuffer->m_UsageFlags & LR_RESOURCE_USAGE_HOST_VISIBLE);

                break;
            }
            case LR_API_ALLOCATOR_DESCRIPTOR:
            {
                pBuffer->m_DataOffset =
                    m_MADescriptor.Allocator.Allocate(pBuffer->m_DeviceDataLen, memoryRequirements.alignment);
                pBuffer->m_pMemoryHandle = m_MADescriptor.pHeap;

                break;
            }
            case LR_API_ALLOCATOR_BUFFER_LINEAR:
            {
                pBuffer->m_DataOffset =
                    m_MABufferLinear.Allocator.Allocate(pBuffer->m_DeviceDataLen, memoryRequirements.alignment);
                pBuffer->m_pMemoryHandle = m_MABufferLinear.pHeap;

                break;
            }
            case LR_API_ALLOCATOR_BUFFER_TLSF:
            {
                Memory::TLSFBlock *pBlock =
                    m_MABufferTLSF.Allocator.Allocate(pBuffer->m_DeviceDataLen, memoryRequirements.alignment);

                pBuffer->m_pAllocatorData = pBlock;
                pBuffer->m_DataOffset = pBlock->m_Offset;
                pBuffer->m_pMemoryHandle = m_MABufferTLSF.pHeap;

                break;
            }
            case LR_API_ALLOCATOR_BUFFER_TLSF_HOST:
            {
                Memory::TLSFBlock *pBlock = m_MABufferTLSFHost.Allocator.Allocate(
                    pBuffer->m_DeviceDataLen, memoryRequirements.alignment);

                pBuffer->m_pAllocatorData = pBlock;
                pBuffer->m_DataOffset = pBlock->m_Offset;
                pBuffer->m_pMemoryHandle = m_MABufferTLSFHost.pHeap;

                break;
            }
            case LR_API_ALLOCATOR_BUFFER_FRAMETIME:
            {
                pBuffer->m_DataOffset = m_MABufferFrametime.Allocator.Allocate(
                    pBuffer->m_DeviceDataLen, memoryRequirements.alignment);
                pBuffer->m_pMemoryHandle = m_MABufferFrametime.pHeap;

                break;
            }
            default:
                LOG_CRITICAL("Trying to allocate VKBuffer resource from invalid allocator.");
                break;
        }
    }

    void VKAPI::SetAllocator(VKImage *pImage, APIAllocatorType targetAllocator)
    {
        ZoneScoped;

        VkMemoryRequirements memoryRequirements = {};
        vkGetImageMemoryRequirements(m_pDevice, pImage->m_pHandle, &memoryRequirements);

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
                pImage->m_DataOffset = pBlock->m_Offset;
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

    void VKAPI::CalcOrthoProjection(Camera2D &camera)
    {
        ZoneScoped;

        camera.m_Projection = XMMatrixOrthographicOffCenterLH(
            0.0, camera.m_ViewSize.x, 0.0, camera.m_ViewSize.y, camera.m_ZNear, camera.m_ZFar);
    }

    void VKAPI::CalcPerspectiveProjection(Camera3D &camera)
    {
        ZoneScoped;

        camera.m_Projection = XMMatrixPerspectiveFovRH(
            XMConvertToRadians(camera.m_FOV), camera.m_Aspect, camera.m_ZNear, camera.m_ZFar);
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

#define _VK_DEFINE_FUNCTION(_name)                                \
    _name = (PFN_##_name)GetProcAddress(m_VulkanLib, #_name);     \
    if (_name == nullptr)                                         \
    {                                                             \
        LOG_CRITICAL("Cannot load vulkan function `{}`!", #_name); \
    }                                                             \
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

        VKCallRet(
            vkCreateInstance(&createInfo, nullptr, &m_pInstance), "Failed to create Vulkan Instance.", false);
        VKCallRet(
            vkCreateWin32SurfaceKHR(m_pInstance, &surfaceInfo, nullptr, &m_pSurface),
            "Failed to create Vulkan Win32 surface.",
            false);

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

                LOG_INFO("\tQueue{} selected as Graphics/Present queue.", i);
                m_AvailableQueueCount++;
                foundGraphicsFamilyIndex = i;

                continue;
            }

            constexpr u32 kComputeFilter = VK_QUEUE_GRAPHICS_BIT;
            if (foundComputeFamilyIndex == -1 && props.queueFlags & VK_QUEUE_COMPUTE_BIT
                && (props.queueFlags & kComputeFilter) == 0)
            {
                LOG_INFO("\tQueue{} selected as Compute queue.", i);
                m_AvailableQueueCount++;
                foundComputeFamilyIndex = i;

                continue;
            }

            constexpr u32 kTransferFilter = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;
            if (foundTransferFamilyIndex == -1 && props.queueFlags & VK_QUEUE_TRANSFER_BIT
                && (props.queueFlags & kTransferFilter) == 0)
            {
                LOG_INFO("\tQueue{} selected as Transfer queue.", i);
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

        LOG_INFO("Checking device extensions:");

        u32 deviceExtensionCount = 0;
        vkEnumerateDeviceExtensionProperties(pPhysicalDevice, nullptr, &deviceExtensionCount, nullptr);

        VkExtensionProperties *pDeviceExtensions = new VkExtensionProperties[deviceExtensionCount];
        vkEnumerateDeviceExtensionProperties(
            pPhysicalDevice, nullptr, &deviceExtensionCount, pDeviceExtensions);

        constexpr eastl::array<const char *, 5> ppRequiredExtensions = {
            "VK_KHR_swapchain",        "VK_KHR_depth_stencil_resolve",   "VK_KHR_dynamic_rendering",
            "VK_KHR_synchronization2", "VK_EXT_extended_dynamic_state2",
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

        vk13Features.synchronization2 = true;
        vk13Features.dynamicRendering = true;

        VkDeviceCreateInfo deviceCreateInfo = {};
        deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

        deviceCreateInfo.pNext = &vk13Features;

        deviceCreateInfo.queueCreateInfoCount = m_AvailableQueueCount;
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

    i64 VKAPI::TFFenceWait(void *pData)
    {
        VKAPI *pAPI = (VKAPI *)pData;
        CommandListPool &commandPool = pAPI->m_CommandListPool;
        auto &directFenceMask = commandPool.m_DirectFenceMask;

        while (true)
        {
            u32 mask = directFenceMask.load(eastl::memory_order_acquire);

            // Iterate over all signaled fences
            while (mask != 0)
            {
                u32 index = Memory::GetLSB(mask);

                CommandList *pList = commandPool.m_DirectLists[index];
                API_VAR(VKCommandList, pList);

                if (pAPI->IsFenceSignaled(pListVK->m_pFence))
                {
                    commandPool.ReleaseFence(index);
                    pAPI->ResetFence(pListVK->m_pFence);
                    commandPool.Release(pList);
                }

                mask ^= 1 << index;
            }
        }

        return 1;
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
        VK_FORMAT_UNDEFINED,
        VK_FORMAT_R32_SFLOAT,               // Float
        VK_FORMAT_R32G32_SFLOAT,            // Vec2
        VK_FORMAT_R32G32B32_SFLOAT,         // Vec3
        VK_FORMAT_B10G11R11_UFLOAT_PACK32,  // Vec3_Packed
        VK_FORMAT_R32G32B32A32_SFLOAT,      // Vec4
        VK_FORMAT_R32_UINT,                 // UInt
        VK_FORMAT_R8G8B8A8_UNORM,           // Vec4U_Packed
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

    VkImageUsageFlags VKAPI::ToVKImageUsage(ResourceUsage usage)
    {
        u32 v = 0;

        if (usage & LR_RESOURCE_USAGE_RENDER_TARGET)
            v |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        if (usage & LR_RESOURCE_USAGE_DEPTH_STENCIL)
            v |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

        if (usage & LR_RESOURCE_USAGE_SHADER_RESOURCE)
            v |= VK_IMAGE_USAGE_SAMPLED_BIT;

        if (usage & LR_RESOURCE_USAGE_TRANSFER_SRC)
            v |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

        if (usage & LR_RESOURCE_USAGE_TRANSFER_DST)
            v |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        if (usage & LR_RESOURCE_USAGE_UNORDERED_ACCESS)
            v |= VK_IMAGE_USAGE_STORAGE_BIT;

        return (VkImageUsageFlags)v;
    }

    VkBufferUsageFlagBits VKAPI::ToVKBufferUsage(ResourceUsage usage)
    {
        u32 v = 0;

        if (usage & LR_RESOURCE_USAGE_VERTEX_BUFFER)
            v |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

        if (usage & LR_RESOURCE_USAGE_INDEX_BUFFER)
            v |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

        if (usage & LR_RESOURCE_USAGE_CONSTANT_BUFFER)
            v |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

        if (usage & LR_RESOURCE_USAGE_TRANSFER_SRC)
            v |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

        if (usage & LR_RESOURCE_USAGE_TRANSFER_DST)
            v |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;

        if (usage & LR_RESOURCE_USAGE_UNORDERED_ACCESS || usage & LR_RESOURCE_USAGE_SHADER_RESOURCE)
            v |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

        return (VkBufferUsageFlagBits)v;
    }

    VkImageLayout VKAPI::ToVKImageLayout(ResourceUsage usage)
    {
        if (usage == LR_RESOURCE_USAGE_PRESENT)
            return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        if (usage == LR_RESOURCE_USAGE_UNKNOWN)
            return VK_IMAGE_LAYOUT_UNDEFINED;

        if (usage & LR_RESOURCE_USAGE_RENDER_TARGET)
            return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        if (usage & LR_RESOURCE_USAGE_DEPTH_STENCIL)
            return VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;

        if (usage & LR_RESOURCE_USAGE_SHADER_RESOURCE)
            return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        if (usage & LR_RESOURCE_USAGE_TRANSFER_SRC)
            return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

        if (usage & LR_RESOURCE_USAGE_TRANSFER_DST)
            return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

        if (usage & LR_RESOURCE_USAGE_UNORDERED_ACCESS)
            return VK_IMAGE_LAYOUT_GENERAL;

        return VK_IMAGE_LAYOUT_UNDEFINED;
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

    VkAccessFlags2 VKAPI::ToVKAccessFlags(PipelineAccess access)
    {
        u32 v = 0;

        if (access == LR_PIPELINE_ACCESS_NONE)
            return VK_ACCESS_2_NONE;

        if (access & LR_PIPELINE_ACCESS_VERTEX_ATTRIB_READ)
            v |= VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT;

        if (access & LR_PIPELINE_ACCESS_INDEX_ATTRIB_READ)
            v |= VK_ACCESS_2_INDEX_READ_BIT;

        if (access & LR_PIPELINE_ACCESS_SHADER_READ)
            v |= VK_ACCESS_2_SHADER_READ_BIT;

        if (access & LR_PIPELINE_ACCESS_SHADER_WRITE)
            v |= VK_ACCESS_2_SHADER_WRITE_BIT;

        if (access & LR_PIPELINE_ACCESS_RENDER_TARGET_READ)
            v |= VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT;

        if (access & LR_PIPELINE_ACCESS_RENDER_TARGET_WRITE)
            v |= VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;

        if (access & LR_PIPELINE_ACCESS_DEPTH_STENCIL_READ)
            v |= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;

        if (access & LR_PIPELINE_ACCESS_DEPTH_STENCIL_WRITE)
            v |= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        if (access & LR_PIPELINE_ACCESS_TRANSFER_READ)
            v |= VK_ACCESS_2_TRANSFER_READ_BIT;

        if (access & LR_PIPELINE_ACCESS_TRANSFER_WRITE)
            v |= VK_ACCESS_2_TRANSFER_WRITE_BIT;

        if (access & LR_PIPELINE_ACCESS_MEMORY_READ)
            v |= VK_ACCESS_2_MEMORY_READ_BIT;

        if (access & LR_PIPELINE_ACCESS_MEMORY_WRITE)
            v |= VK_ACCESS_2_MEMORY_WRITE_BIT;

        if (access & LR_PIPELINE_ACCESS_HOST_READ)
            v |= VK_ACCESS_2_HOST_READ_BIT;

        if (access & LR_PIPELINE_ACCESS_HOST_WRITE)
            v |= VK_ACCESS_2_HOST_WRITE_BIT;

        return (VkAccessFlags2)v;
    }

}  // namespace lr::Graphics