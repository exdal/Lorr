#include "VKAPI.hh"

#include "Engine.hh"

#include "Core/Crypto/Light.hh"
#include "Core/IO/Memory.hh"

#include "VKCommandList.hh"
#include "VKShader.hh"

/// DEFINE VULKAN FUNCTIONS
// #include "VKSymbols.hh"
#define _VK_DEFINE_FUNCTION(_name) PFN_##_name _name
// Define symbols
_VK_IMPORT_SYMBOLS
#undef _VK_DEFINE_FUNCTION

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

// Example: `BaseCommandQueue *pQueue` -> `VKCommandQueue *pQueueVK`
#define API_VAR(type, name) type *name##VK = (type *)name

namespace lr::Graphics
{
    bool VKAPI::Init(PlatformWindow *pWindow, u32 width, u32 height, APIFlags flags)
    {
        ZoneScoped;

        LOG_TRACE("Initializing Vulkan device...");

        LoadVulkan();
        SetupInstance(pWindow->m_Handle);

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

        vkGetPhysicalDeviceMemoryProperties(m_pPhysicalDevice, &m_MemoryProps);

        // yeah uh, dont delete
        // delete[] ppAvailableDevices;

        InitAllocators();

        m_CommandListPool.Init();
        m_pDescriptorPool = CreateDescriptorPool({
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 256 },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 256 },
        });

        m_pPipelineCache = CreatePipelineCache();

        m_pDirectQueue = (VKCommandQueue *)CreateCommandQueue(CommandListType::Direct);
        m_SwapChain.Init(pWindow, this, SwapChainFlags::TripleBuffering);
        m_SwapChain.CreateBackBuffers(this);

        LOG_TRACE("Successfully initialized Vulkan context.");

        InitCommandLists();

        m_FenceThread.Begin(VKAPI::TFFenceWait, this);

        LOG_TRACE("Initialized Vulkan render context.");

        return true;
    }

    void VKAPI::InitAllocators()
    {
        ZoneScoped;

        constexpr u32 kTypeMem = Memory::MiBToBytes(16);

        m_TypeAllocator.Init(kTypeMem);
        m_pTypeData = Memory::Allocate<u8>(kTypeMem);

        constexpr u32 kDescriptorMem = Memory::MiBToBytes(1);
        constexpr u32 kBufferLinearMem = Memory::MiBToBytes(64);
        constexpr u32 kBufferTLSFMem = Memory::MiBToBytes(512);
        constexpr u32 kImageTLSFMem = Memory::MiBToBytes(512);

        m_MADescriptor.Allocator.AddPool(kDescriptorMem);
        m_MADescriptor.pHeap = CreateHeap(kDescriptorMem, true);

        m_MABufferLinear.Allocator.AddPool(kBufferLinearMem);
        m_MABufferLinear.pHeap = CreateHeap(kBufferLinearMem, true);

        m_MABufferTLSF.Allocator.Init(kBufferTLSFMem);
        m_MABufferTLSF.pHeap = CreateHeap(kBufferTLSFMem, false);

        m_MAImageTLSF.Allocator.Init(kImageTLSFMem);
        m_MAImageTLSF.pHeap = CreateHeap(kImageTLSFMem, false);
    }

    BaseCommandQueue *VKAPI::CreateCommandQueue(CommandListType type)
    {
        ZoneScoped;

        VKCommandQueue *pQueue = AllocTypeInherit<BaseCommandQueue, VKCommandQueue>();

        VkQueue pHandle = nullptr;
        VkFence pFence = CreateFence(false);

        vkGetDeviceQueue(m_pDevice, m_QueueIndexes[(u32)type], 0, &pHandle);

        pQueue->Init(pHandle, pFence);

        return pQueue;
    }

    BaseCommandAllocator *VKAPI::CreateCommandAllocator(CommandListType type)
    {
        ZoneScoped;

        VKCommandAllocator *pAllocator = AllocTypeInherit<BaseCommandAllocator, VKCommandAllocator>();

        VkCommandPoolCreateInfo allocatorInfo = {};
        allocatorInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        allocatorInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        allocatorInfo.queueFamilyIndex = m_QueueIndexes[(u32)type];

        vkCreateCommandPool(m_pDevice, &allocatorInfo, nullptr, &pAllocator->pHandle);

        return pAllocator;
    }

    BaseCommandList *VKAPI::CreateCommandList(CommandListType type)
    {
        ZoneScoped;

        BaseCommandAllocator *pAllocator = CreateCommandAllocator(type);
        API_VAR(VKCommandAllocator, pAllocator);

        VKCommandList *pList = AllocTypeInherit<BaseCommandList, VKCommandList>();

        VkCommandBuffer pHandle = nullptr;
        VkFence pFence = CreateFence(false);

        VkCommandBufferAllocateInfo listInfo = {};
        listInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        listInfo.commandBufferCount = 1;
        listInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        listInfo.commandPool = pAllocatorVK->pHandle;

        vkAllocateCommandBuffers(m_pDevice, &listInfo, &pHandle);

        pList->Init(pHandle, pFence, type);

        return pList;
    }

    void VKAPI::BeginCommandList(BaseCommandList *pList)
    {
        ZoneScoped;

        API_VAR(VKCommandList, pList);

        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.pNext = nullptr;
        beginInfo.flags = 0;

        vkBeginCommandBuffer(pListVK->m_pHandle, &beginInfo);
    }

    void VKAPI::EndCommandList(BaseCommandList *pList)
    {
        ZoneScoped;

        API_VAR(VKCommandList, pList);

        vkEndCommandBuffer(pListVK->m_pHandle);
    }

    void VKAPI::ResetCommandAllocator(BaseCommandAllocator *pAllocator)
    {
        ZoneScoped;

        API_VAR(VKCommandAllocator, pAllocator);

        vkResetCommandPool(m_pDevice, pAllocatorVK->pHandle, 0);
    }

    void VKAPI::ExecuteCommandList(BaseCommandList *pList, bool waitForFence)
    {
        ZoneScoped;

        API_VAR(VKCommandList, pList);

        VkQueue &pQueueHandle = m_pDirectQueue->m_pHandle;
        VkFence &pFence = pListVK->m_pFence;

        VkSubmitInfo2 submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;

        VkCommandBufferSubmitInfo commandBufferInfo = {};
        commandBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
        commandBufferInfo.commandBuffer = pListVK->m_pHandle;

        submitInfo.commandBufferInfoCount = 1;
        submitInfo.pCommandBufferInfos = &commandBufferInfo;

        // ~~INVESTIGATE: At the start of submission, we get threading errors from VL.~~
        // Found the issue, fence mask always set to least set bit in `CommandListPool` (commit `ac0bc8f`)
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

        VkSemaphore &pAcquireSemp = pCurrentFrame->pAcquireSemp;
        VkSemaphore &pPresentSemp = pCurrentFrame->pPresentSemp;

        VkSubmitInfo2 submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;

        submitInfo.commandBufferInfoCount = 0;

        VkSemaphoreSubmitInfo acquireSemaphoreInfo = {};
        acquireSemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        acquireSemaphoreInfo.semaphore = pAcquireSemp;
        acquireSemaphoreInfo.stageMask = m_pDirectQueue->m_AcquireStage;

        VkSemaphoreSubmitInfo presentSemaphoreInfo = {};
        presentSemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        presentSemaphoreInfo.semaphore = pPresentSemp;
        presentSemaphoreInfo.stageMask = m_pDirectQueue->m_PresentStage;

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

    GraphicsPipelineBuildInfo *VKAPI::BeginPipelineBuildInfo()
    {
        ZoneScoped;

        GraphicsPipelineBuildInfo *pBuildInfo = new VKGraphicsPipelineBuildInfo;
        pBuildInfo->Init();

        return pBuildInfo;
    }

    BasePipeline *VKAPI::EndPipelineBuildInfo(GraphicsPipelineBuildInfo *pBuildInfo)
    {
        ZoneScoped;

        API_VAR(VKGraphicsPipelineBuildInfo, pBuildInfo);
        VKPipeline *pPipeline = AllocTypeInherit<BasePipeline, VKPipeline>();

        VkPipeline &pHandle = pPipeline->pHandle;
        VkPipelineLayout &pLayout = pPipeline->pLayout;

        VkDescriptorSetLayout pSetLayouts[8] = {};
        for (u32 i = 0; i < pBuildInfo->m_DescriptorSetCount; i++)
        {
            BaseDescriptorSet *pSet = pBuildInfo->m_ppDescriptorSets[i];
            API_VAR(VKDescriptorSet, pSet);

            pSetLayouts[i] = pSetVK->pSetLayoutHandle;
        }

        VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
        pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutCreateInfo.setLayoutCount = pBuildInfo->m_DescriptorSetCount;
        pipelineLayoutCreateInfo.pSetLayouts = pSetLayouts;

        vkCreatePipelineLayout(m_pDevice, &pipelineLayoutCreateInfo, nullptr, &pLayout);

        pBuildInfoVK->m_CreateInfo.layout = pLayout;

        vkCreateGraphicsPipelines(m_pDevice, m_pPipelineCache, 1, &pBuildInfoVK->m_CreateInfo, nullptr, &pHandle);

        delete pBuildInfoVK;

        return pPipeline;
    }

    void VKAPI::CreateSwapChain(VkSwapchainKHR &pHandle, VkSwapchainCreateInfoKHR &info)
    {
        ZoneScoped;

        VkResult vkResult;
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

    void VKAPI::Frame()
    {
        ZoneScoped;

        VkSwapchainKHR &pSwapChain = m_SwapChain.m_pHandle;

        VkQueue &pQueueHandle = m_pDirectQueue->m_pHandle;

        VKSwapChainFrame *pCurrentFrame = m_SwapChain.GetCurrentFrame();
        VkSemaphore &pAcquireSemp = pCurrentFrame->pAcquireSemp;
        VkSemaphore &pPresentSemp = pCurrentFrame->pPresentSemp;

        u32 imageIndex;
        vkAcquireNextImageKHR(m_pDevice, pSwapChain, UINT64_MAX, pAcquireSemp, nullptr, &imageIndex);

        m_pDirectQueue->SetSemaphoreStage(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

        PresentCommandQueue();

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
    }

    void VKAPI::WaitForDevice()
    {
        ZoneScoped;

        vkDeviceWaitIdle(m_pDevice);
    }

    BaseShader *VKAPI::CreateShader(ShaderStage stage, BufferReadStream &buf)
    {
        ZoneScoped;

        VKShader *pShader = AllocTypeInherit<BaseShader, VKShader>();

        VkShaderModule pHandle = nullptr;

        VkShaderModuleCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = buf.GetLength();
        createInfo.pCode = (u32 *)buf.GetData();

        vkCreateShaderModule(m_pDevice, &createInfo, nullptr, &pHandle);

        pShader->Type = stage;
        pShader->pHandle = pHandle;

        return pShader;
    }

    BaseShader *VKAPI::CreateShader(ShaderStage stage, eastl::string_view path)
    {
        ZoneScoped;

        FileStream fs(path, false);
        void *pData = fs.ReadAll<void>();
        fs.Close();

        BufferReadStream buf(pData, fs.Size());
        BaseShader *pShader = CreateShader(stage, buf);

        free(pData);

        return pShader;
    }

    void VKAPI::DeleteShader(BaseShader *pShader)
    {
        ZoneScoped;

        API_VAR(VKShader, pShader);

        vkDestroyShaderModule(m_pDevice, pShaderVK->pHandle, nullptr);

        FreeType(pShaderVK);
    }

    BaseDescriptorSet *VKAPI::CreateDescriptorSet(DescriptorSetDesc *pDesc)
    {
        ZoneScoped;

        VKDescriptorSet *pDescriptorSet = AllocTypeInherit<BaseDescriptorSet, VKDescriptorSet>();

        VkDescriptorSetLayoutBinding pBindings[8] = {};

        for (u32 i = 0; i < pDesc->BindingCount; i++)
        {
            DescriptorBindingDesc &element = pDesc->pBindings[i];

            pBindings[i].binding = i;
            pBindings[i].descriptorType = ToVKDescriptorType(element.Type);
            pBindings[i].stageFlags = ToVKShaderType(element.TargetShader);
            pBindings[i].descriptorCount = element.ArraySize;
        }

        // First we need layout info
        VkDescriptorSetLayoutCreateInfo layoutCreateInfo = {};
        layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutCreateInfo.bindingCount = pDesc->BindingCount;
        layoutCreateInfo.pBindings = pBindings;

        vkCreateDescriptorSetLayout(m_pDevice, &layoutCreateInfo, nullptr, &pDescriptorSet->pSetLayoutHandle);

        // And the actual set
        VkDescriptorSetAllocateInfo allocateCreateInfo = {};
        allocateCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocateCreateInfo.descriptorPool = m_pDescriptorPool;
        allocateCreateInfo.descriptorSetCount = 1;
        allocateCreateInfo.pSetLayouts = &pDescriptorSet->pSetLayoutHandle;

        vkAllocateDescriptorSets(m_pDevice, &allocateCreateInfo, &pDescriptorSet->pHandle);

        /// ---------------------------------------------------------- ///

        pDescriptorSet->BindingCount = pDesc->BindingCount;
        memcpy(pDescriptorSet->pBindingInfos, pDesc->pBindings, pDesc->BindingCount * sizeof(DescriptorBindingDesc));

        return pDescriptorSet;
    }

    void VKAPI::UpdateDescriptorData(BaseDescriptorSet *pSet)
    {
        ZoneScoped;

        API_VAR(VKDescriptorSet, pSet);

        VkWriteDescriptorSet pWriteSets[8];
        VkDescriptorBufferInfo pBufferInfos[8];
        VkDescriptorImageInfo pImageInfos[8];

        u32 bufferIndex = 0;
        u32 imageIndex = 0;

        for (u32 i = 0; i < pSetVK->BindingCount; i++)
        {
            DescriptorBindingDesc &element = pSetVK->pBindingInfos[i];

            BaseBuffer *pBuffer = element.pBuffer;
            BaseImage *pImage = element.pImage;

            API_VAR(VKBuffer, pBuffer);
            API_VAR(VKImage, pImage);

            if (element.Type == DescriptorType::ConstantBufferView)
            {
                pBufferInfos[bufferIndex].buffer = pBufferVK->m_pHandle;
                pBufferInfos[bufferIndex].offset = 0;
                pBufferInfos[bufferIndex].range = pBufferVK->m_DataSize;

                pWriteSets[i].pBufferInfo = &pBufferInfos[i];

                bufferIndex++;
            }
            else if (element.Type == DescriptorType::ShaderResourceView)
            {
                pImageInfos[imageIndex].sampler = pImageVK->m_pSampler;
                pImageInfos[imageIndex].imageView = pImageVK->m_pViewHandle;
                pImageInfos[imageIndex].imageLayout = pImageVK->m_Layout;

                pWriteSets[i].pImageInfo = &pImageInfos[i];

                imageIndex++;
            }

            pWriteSets[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            pWriteSets[i].pNext = nullptr;
            pWriteSets[i].dstSet = pSetVK->pHandle;
            pWriteSets[i].dstBinding = i;
            pWriteSets[i].dstArrayElement = 0;
            pWriteSets[i].descriptorCount = element.ArraySize;
            pWriteSets[i].descriptorType = ToVKDescriptorType(element.Type);
        }

        vkUpdateDescriptorSets(m_pDevice, pSetVK->BindingCount, pWriteSets, 0, nullptr);
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
            pPoolSizes[idx].type = element.Type;
            pPoolSizes[idx].descriptorCount = element.Count;
            maxSets += element.Count;

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
            cpuWrite ? VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

        VkMemoryAllocateInfo memoryAllocInfo = {};
        memoryAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        memoryAllocInfo.allocationSize = heapSize;
        memoryAllocInfo.memoryTypeIndex = GetMemoryTypeIndex(memProps);

        VkDeviceMemory pHandle = nullptr;
        vkAllocateMemory(m_pDevice, &memoryAllocInfo, nullptr, &pHandle);

        return pHandle;
    }

    BaseBuffer *VKAPI::CreateBuffer(BufferDesc *pDesc, BufferData *pData)
    {
        ZoneScoped;

        VKBuffer *pBuffer = AllocTypeInherit<BaseBuffer, VKBuffer>();

        VkBufferCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        createInfo.size = pData->DataLen;
        createInfo.usage = ToVKBufferUsage(pDesc->UsageFlags);

        vkCreateBuffer(m_pDevice, &createInfo, nullptr, &pBuffer->m_pHandle);

        VkMemoryRequirements memoryRequirements = {};
        vkGetBufferMemoryRequirements(m_pDevice, pBuffer->m_pHandle, &memoryRequirements);

        /// ---------------------------------------------- ///

        pBuffer->m_RequiredDataSize = memoryRequirements.size;
        pBuffer->m_Usage = pDesc->UsageFlags;
        pBuffer->m_Mappable = pDesc->Mappable;
        pBuffer->m_DataSize = pData->DataLen;
        pBuffer->m_AllocatorType = pDesc->TargetAllocator;
        pBuffer->m_Stride = pData->Stride;

        /// ---------------------------------------------- ///

        switch (pDesc->TargetAllocator)
        {
            case AllocatorType::None:
            {
                pBuffer->m_pMemoryHandle = CreateHeap(memoryRequirements.size, pBuffer->m_Mappable);

                break;
            }

            case AllocatorType::Descriptor:
            {
                pBuffer->m_DataOffset = m_MADescriptor.Allocator.Allocate(0, pBuffer->m_RequiredDataSize, memoryRequirements.alignment);
                pBuffer->m_pMemoryHandle = m_MADescriptor.pHeap;

                break;
            }

            case AllocatorType::BufferLinear:
            {
                pBuffer->m_DataOffset = m_MABufferLinear.Allocator.Allocate(0, pBuffer->m_RequiredDataSize, memoryRequirements.alignment);
                pBuffer->m_pMemoryHandle = m_MABufferLinear.pHeap;

                break;
            }

            case AllocatorType::BufferTLSF:
            {
                Memory::TLSFBlock *pBlock = m_MABufferTLSF.Allocator.Allocate(pBuffer->m_RequiredDataSize, memoryRequirements.alignment);

                pBuffer->m_pAllocatorData = pBlock;
                pBuffer->m_DataOffset = pBlock->Offset;

                pBuffer->m_pMemoryHandle = m_MABufferTLSF.pHeap;

                break;
            }

            default: break;
        }

        vkBindBufferMemory(m_pDevice, pBuffer->m_pHandle, pBuffer->m_pMemoryHandle, pBuffer->m_DataOffset);

        return pBuffer;
    }

    void VKAPI::DeleteBuffer(BaseBuffer *pHandle)
    {
        ZoneScoped;

        API_VAR(VKBuffer, pHandle);

        if (pHandleVK->m_pMemoryHandle)
        {
            if (pHandleVK->m_AllocatorType == AllocatorType::BufferTLSF)
            {
                Memory::TLSFBlock *pBlock = (Memory::TLSFBlock *)pHandleVK->m_pAllocatorData;
                assert(pBlock != nullptr);

                m_MABufferTLSF.Allocator.Free(pBlock);
            }
            else if (pHandleVK->m_AllocatorType == AllocatorType::None)
            {
                vkFreeMemory(m_pDevice, pHandleVK->m_pMemoryHandle, nullptr);
            }
        }

        if (pHandleVK->m_pHandle)
            vkDestroyBuffer(m_pDevice, pHandleVK->m_pHandle, nullptr);
    }

    void VKAPI::MapMemory(BaseBuffer *pBuffer, void *&pData)
    {
        ZoneScoped;

        API_VAR(VKBuffer, pBuffer);

        vkMapMemory(m_pDevice, pBufferVK->m_pMemoryHandle, pBuffer->m_DataOffset, pBuffer->m_RequiredDataSize, 0, &pData);
    }

    void VKAPI::UnmapMemory(BaseBuffer *pBuffer)
    {
        ZoneScoped;

        API_VAR(VKBuffer, pBuffer);

        vkUnmapMemory(m_pDevice, pBufferVK->m_pMemoryHandle);
    }

    BaseBuffer *VKAPI::ChangeAllocator(BaseCommandList *pList, BaseBuffer *pTarget, AllocatorType targetAllocator)
    {
        ZoneScoped;

        // TODO: Currently this technique uses present queue, move it to transfer queue

        BufferDesc bufDesc = {};
        BufferData bufData = {};

        bufDesc.Mappable = false;
        bufDesc.UsageFlags = pTarget->m_Usage | ResourceUsage::CopyDst;
        bufDesc.UsageFlags &= ~ResourceUsage::CopySrc;

        bufData.DataLen = pTarget->m_DataSize;

        BaseBuffer *pNewBuffer = CreateBuffer(&bufDesc, &bufData);

        pList->CopyBuffer(pTarget, pNewBuffer, bufData.DataLen);

        return pNewBuffer;
    }

    BaseImage *VKAPI::CreateImage(ImageDesc *pDesc, ImageData *pData)
    {
        ZoneScoped;

        VKImage *pImage = AllocTypeInherit<BaseImage, VKImage>();

        VkImageCreateInfo imageCreateInfo = {};
        imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageCreateInfo.format = ToVKFormat(pDesc->Format);
        imageCreateInfo.usage = ToVKImageUsage(pDesc->Usage);
        imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;

        imageCreateInfo.extent.width = pData->Width;
        imageCreateInfo.extent.height = pData->Height;
        imageCreateInfo.extent.depth = 1;

        imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageCreateInfo.mipLevels = pDesc->MipMapLevels;
        imageCreateInfo.arrayLayers = pDesc->ArraySize;
        imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;

        vkCreateImage(m_pDevice, &imageCreateInfo, nullptr, &pImage->m_pHandle);

        VkMemoryRequirements memoryRequirements = {};
        vkGetImageMemoryRequirements(m_pDevice, pImage->m_pHandle, &memoryRequirements);

        /// ---------------------------------------------- //

        pImage->m_Width = pData->Width;
        pImage->m_Height = pData->Height;
        pImage->m_DataSize = pData->DataLen;

        pImage->m_UsingMip = 0;
        pImage->m_TotalMips = pDesc->MipMapLevels;

        pImage->m_Usage = pDesc->Usage;
        pImage->m_Format = pDesc->Format;

        pImage->m_RequiredDataSize = memoryRequirements.size;

        pImage->m_AllocatorType = pDesc->TargetAllocator;

        /// ---------------------------------------------- //

        switch (pDesc->TargetAllocator)
        {
            case AllocatorType::None:
            {
                pImage->m_pMemoryHandle = CreateHeap(memoryRequirements.size, false);

                break;
            }

            case AllocatorType::ImageTLSF:
            {
                Memory::TLSFBlock *pBlock = m_MABufferTLSF.Allocator.Allocate(pImage->m_DataSize, memoryRequirements.alignment);

                pImage->m_pAllocatorData = pBlock;
                pImage->m_DataOffset = pBlock->Offset;

                pImage->m_pMemoryHandle = m_MAImageTLSF.pHeap;

                break;
            }

            default: break;
        }

        vkBindImageMemory(m_pDevice, pImage->m_pHandle, pImage->m_pMemoryHandle, 0);

        return pImage;
    }

    void VKAPI::DeleteImage(BaseImage *pImage)
    {
        ZoneScoped;

        API_VAR(VKImage, pImage);

        if (pImageVK->m_pMemoryHandle)
        {
            if (pImage->m_AllocatorType == AllocatorType::ImageTLSF)
            {
                Memory::TLSFBlock *pBlock = (Memory::TLSFBlock *)pImage->m_pAllocatorData;
                assert(pBlock != nullptr);

                m_MAImageTLSF.Allocator.Free(pBlock);
            }
            else if (pImage->m_AllocatorType == AllocatorType::None)
            {
                vkFreeMemory(m_pDevice, pImageVK->m_pMemoryHandle, nullptr);
            }
        }

        if (pImageVK->m_pViewHandle)
            vkDestroyImageView(m_pDevice, pImageVK->m_pViewHandle, nullptr);

        if (pImageVK->m_pSampler)
            vkDestroySampler(m_pDevice, pImageVK->m_pSampler, nullptr);

        if (pImageVK->m_pHandle)
            vkDestroyImage(m_pDevice, pImageVK->m_pHandle, nullptr);
    }

    void VKAPI::CreateImageView(BaseImage *pHandle)
    {
        ZoneScoped;

        API_VAR(VKImage, pHandle);

        VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;

        if (pHandle->m_Usage & ResourceUsage::DepthStencilWrite)
        {
            aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        }

        VkImageViewCreateInfo imageViewCreateInfo = {};
        imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;

        imageViewCreateInfo.image = pHandleVK->m_pHandle;
        imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        imageViewCreateInfo.format = ToVKFormat(pHandle->m_Format);

        imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

        imageViewCreateInfo.subresourceRange.aspectMask = aspectFlags;
        imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
        imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
        imageViewCreateInfo.subresourceRange.levelCount = 1;
        imageViewCreateInfo.subresourceRange.layerCount = 1;

        vkCreateImageView(m_pDevice, &imageViewCreateInfo, nullptr, &pHandleVK->m_pViewHandle);
    }

    void VKAPI::CreateSampler(BaseImage *pHandle)
    {
        ZoneScoped;

        API_VAR(VKImage, pHandle);

        VkSamplerCreateInfo samplerInfo = {};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.pNext = nullptr;

        samplerInfo.flags = 0;

        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;

        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;

        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.mipLodBias = 0.0;
        samplerInfo.maxLod = pHandle->m_UsingMip;
        samplerInfo.minLod = 0.0;

        samplerInfo.compareEnable = false;
        samplerInfo.compareOp = VK_COMPARE_OP_NEVER;

        samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;

        vkCreateSampler(m_pDevice, &samplerInfo, nullptr, &pHandleVK->m_pSampler);
    }

    void VKAPI::CreateRenderTarget(BaseImage *pImage)
    {
        ZoneScoped;
    }

    bool VKAPI::IsFormatSupported(ResourceFormat format, VkColorSpaceKHR *pColorSpaceOut)
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

#if _DEBUG

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

#if _DEBUG
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

        vk13Features.synchronization2 = true;
        vk13Features.dynamicRendering = true;

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

                BaseCommandList *pList = commandPool.m_DirectLists[index];
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
        VK_FORMAT_UNDEFINED,             // Unknown
        VK_FORMAT_BC1_RGBA_UNORM_BLOCK,  // BC1
        VK_FORMAT_R8G8B8A8_UNORM,        // RGBA8F
        VK_FORMAT_R8G8B8A8_SRGB,         // RGBA8_SRGBF
        VK_FORMAT_B8G8R8A8_UNORM,        // BGRA8F
        VK_FORMAT_R16G16B16A16_SFLOAT,   // RGBA16F
        VK_FORMAT_R32G32B32A32_SFLOAT,   // RGBA32F
        VK_FORMAT_R32_UINT,              // R32U
        VK_FORMAT_R32_SFLOAT,            // R32F
        VK_FORMAT_D32_SFLOAT,            // D32F
        VK_FORMAT_D32_SFLOAT_S8_UINT,    // D32FS8U
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
        VK_DESCRIPTOR_TYPE_SAMPLER,                 // Sampler
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,  // ShaderResourceView
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,          // ConstantBufferView
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,          // UnorderedAccessBuffer
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,           // UnorderedAccessView
        VK_DESCRIPTOR_TYPE_MAX_ENUM,                // RootConstant
    };

    VkFormat VKAPI::ToVKFormat(ResourceFormat format)
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

        if (usage & ResourceUsage::RenderTarget)
            v |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        if (usage & ResourceUsage::DepthStencilWrite || usage & ResourceUsage::DepthStencilRead)
            v |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

        if (usage & ResourceUsage::ShaderResource)
            v |= VK_IMAGE_USAGE_SAMPLED_BIT;

        if (usage & ResourceUsage::CopySrc)
            v |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

        if (usage & ResourceUsage::CopyDst)
            v |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        return (VkImageUsageFlags)v;
    }

    VkBufferUsageFlagBits VKAPI::ToVKBufferUsage(ResourceUsage usage)
    {
        u32 v = 0;

        if (usage & ResourceUsage::VertexBuffer)
            v |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

        if (usage & ResourceUsage::IndexBuffer)
            v |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

        if (usage & ResourceUsage::ConstantBuffer)
            v |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

        if (usage & ResourceUsage::UnorderedAccess)
            v |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

        if (usage & ResourceUsage::IndirectBuffer)
            v |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;

        if (usage & ResourceUsage::CopySrc)
            v |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

        if (usage & ResourceUsage::CopyDst)
            v |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;

        return (VkBufferUsageFlagBits)v;
    }

    VkImageLayout VKAPI::ToVKImageLayout(ResourceUsage usage)
    {
        u32 v = 0;

        if (usage & ResourceUsage::Undefined)
            v |= VK_IMAGE_LAYOUT_UNDEFINED;

        if (usage & ResourceUsage::RenderTarget)
            v |= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        if (usage & ResourceUsage::DepthStencilWrite)
            v |= VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;

        if (usage & ResourceUsage::DepthStencilRead)
            v |= VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

        if (usage & ResourceUsage::ShaderResource)
            v |= VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        if (usage & ResourceUsage::CopySrc)
            v |= VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

        if (usage & ResourceUsage::CopyDst)
            v |= VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

        if (usage & ResourceUsage::Present)
            v |= VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        if (usage & ResourceUsage::UnorderedAccess)
            v |= VK_IMAGE_LAYOUT_GENERAL;

        return (VkImageLayout)v;
    }

    VkShaderStageFlagBits VKAPI::ToVKShaderType(ShaderStage type)
    {
        u32 v = 0;

        if (type & ShaderStage::Vertex)
            v |= VK_SHADER_STAGE_VERTEX_BIT;

        if (type & ShaderStage::Pixel)
            v |= VK_SHADER_STAGE_FRAGMENT_BIT;

        if (type & ShaderStage::Compute)
            v |= VK_SHADER_STAGE_COMPUTE_BIT;

        if (type & ShaderStage::Hull)
            v |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;

        if (type & ShaderStage::Domain)
            v |= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;

        if (type & ShaderStage::Geometry)
            v |= VK_SHADER_STAGE_GEOMETRY_BIT;

        return (VkShaderStageFlagBits)v;
    }

    VkPipelineStageFlags VKAPI::ToVKPipelineStage(ResourceUsage usage)
    {
        u32 v = 0;

        if (usage & ResourceUsage::Undefined)
            v |= VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

        if (usage & ResourceUsage::VertexBuffer || usage & ResourceUsage::IndexBuffer)
            v |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;

        if (usage & ResourceUsage::RenderTarget)
            v |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

        if (usage & ResourceUsage::DepthStencilWrite || usage & ResourceUsage::DepthStencilRead)
            v |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

        if (usage & ResourceUsage::CopySrc || usage & ResourceUsage::CopyDst)
            v |= VK_PIPELINE_STAGE_TRANSFER_BIT;

        if (usage & ResourceUsage::Present)
            v |= VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

        if (usage & ResourceUsage::IndirectBuffer)
            v |= VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;

        return (VkPipelineStageFlagBits)v;
    }

    VkPipelineStageFlags VKAPI::ToVKPipelineShaderStage(ShaderStage type)
    {
        u32 v = 0;

        if (type & ShaderStage::Vertex)
            v |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;

        if (type & ShaderStage::Pixel)
            v |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

        if (type & ShaderStage::Compute)
            v |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

        if (type & ShaderStage::Hull)
            v |= VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT;

        if (type & ShaderStage::Domain)
            v |= VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT;

        if (type & ShaderStage::Geometry)
            v |= VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT;

        return (VkPipelineStageFlagBits)v;
    }

    VkAccessFlags VKAPI::ToVKAccessFlags(ResourceUsage usage)
    {
        u32 v = 0;

        if (usage & ResourceUsage::Undefined)
            v |= VK_ACCESS_NONE;

        if (usage & ResourceUsage::VertexBuffer)
            v |= VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;

        if (usage & ResourceUsage::IndexBuffer)
            v |= VK_ACCESS_INDEX_READ_BIT;

        if (usage & ResourceUsage::IndirectBuffer)
            v |= VK_ACCESS_INDIRECT_COMMAND_READ_BIT;

        if (usage & ResourceUsage::ConstantBuffer)
            v |= VK_ACCESS_UNIFORM_READ_BIT;

        if (usage & ResourceUsage::ShaderResource)
            v |= VK_ACCESS_SHADER_READ_BIT;

        if (usage & ResourceUsage::RenderTarget)
            v |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        if (usage & ResourceUsage::DepthStencilWrite)
            v |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;

        if (usage & ResourceUsage::DepthStencilRead)
            v |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        if (usage & ResourceUsage::CopySrc)
            v |= VK_ACCESS_TRANSFER_READ_BIT;

        if (usage & ResourceUsage::CopyDst)
            v |= VK_ACCESS_TRANSFER_WRITE_BIT;

        if (usage & ResourceUsage::Present)
            v |= VK_ACCESS_MEMORY_READ_BIT;

        return (VkAccessFlags)v;
    }

}  // namespace lr::Graphics