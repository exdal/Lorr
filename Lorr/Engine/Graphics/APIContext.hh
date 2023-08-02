// Created on Monday July 18th 2022 by exdal
// Last modified on Wednesday August 2nd 2023 by exdal

#pragma once

#include "IO/BufferStream.hh"
#include "Memory/Allocator/LinearAllocator.hh"
#include "Memory/Allocator/TLSFAllocator.hh"
#include "Resource/Resource.hh"

#include "Device.hh"
#include "CommandQueue.hh"
#include "CommandList.hh"
#include "Pipeline.hh"
#include "SwapChain.hh"

#include "TracyVK.hh"

#ifdef CreateSemaphore
#undef CreateSemaphore
#endif

namespace lr::Graphics
{
enum APIFlags : u32
{
    LR_API_FLAG_NONE = 0,
    LR_API_FLAG_VSYNC = 1 << 0,
};
EnumFlags(APIFlags);

enum class MemoryFlag
{
    Device = 1 << 0,
    HostVisible = 1 << 1,
    HostCoherent = 1 << 2,
    HostCached = 1 << 3,

    HostVisibleCoherent = HostVisible | HostCoherent,
};
EnumFlags(MemoryFlag);

struct APIContextDesc
{
    APIFlags m_Flags = LR_API_FLAG_NONE;
    u32 m_ImageCount = 1;
    BaseWindow *m_pTargetWindow = nullptr;
    ResourceAllocatorDesc m_AllocatorDesc = {};
};

struct SubmitDesc
{
    CommandType m_Type = CommandType::Count;
    eastl::span<SemaphoreSubmitDesc> m_WaitSemas;
    eastl::span<CommandListSubmitDesc> m_Lists;
    eastl::span<SemaphoreSubmitDesc> m_SignalSemas;
};

struct APIContext
{
    bool Init(APIContextDesc *pDesc);

    /// COMMAND ///
    u32 GetQueueIndex(CommandType type);
    CommandQueue *CreateCommandQueue(CommandType type);
    CommandAllocator *CreateCommandAllocator(CommandType type, bool resetLists);
    CommandList *CreateCommandList(CommandType type, CommandAllocator *pAllocator = nullptr);

    void AllocateCommandList(CommandList *pList, CommandAllocator *pAllocator);
    void BeginCommandList(CommandList *pList);
    void EndCommandList(CommandList *pList);
    void ResetCommandAllocator(CommandAllocator *pAllocator);
    void Submit(SubmitDesc *pDesc);

    Fence *CreateFence(bool signaled);
    void DeleteFence(Fence *pFence);
    bool IsFenceSignaled(Fence *pFence);
    void ResetFence(Fence *pFence);

    Semaphore *CreateSemaphore(u32 initialValue = 0, bool binary = true);
    void DeleteSemaphore(Semaphore *pSemaphore);
    void WaitForSemaphore(Semaphore *pSemaphore, u64 desiredValue, u64 timeout = UINT64_MAX);

    /// PIPELINE ///
    VkPipelineCache CreatePipelineCache(u32 initialDataSize = 0, void *pInitialData = nullptr);
    PipelineLayout *CreatePipelineLayout(
        eastl::span<DescriptorSetLayout *> layouts, eastl::span<PushConstantDesc> pushConstants);

    Pipeline *CreateGraphicsPipeline(GraphicsPipelineBuildInfo *pBuildInfo);
    Pipeline *CreateComputePipeline(ComputePipelineBuildInfo *pBuildInfo);

    /// SWAPCHAIN ///
    SwapChain *CreateSwapChain(BaseWindow *pWindow, u32 imageCount, SwapChain *pOldSwapChain);
    void DeleteSwapChain(SwapChain *pSwapChain, bool keepSelf);
    void ResizeSwapChain(BaseWindow *pWindow);
    SwapChain *GetSwapChain();

    void WaitForWork();
    void BeginFrame();
    void EndFrame();

    /// RESOURCE ///
    u64 GetBufferMemorySize(Buffer *pBuffer, u64 *pAlignmentOut = nullptr);
    u64 GetImageMemorySize(Image *pImage, u64 *pAlignmentOut = nullptr);

    LinearDeviceMemory *CreateLinearAllocator(MemoryFlag memoryFlags, u64 memSize);
    TLSFDeviceMemory *CreateTLSFAllocator(MemoryFlag memoryFlags, u64 memSize, u32 maxAllocs);
    void DeleteAllocator(DeviceMemory *pDeviceMemory);
    u64 OffsetFrametimeMemory(u64 offset);
    void AllocateBufferMemory(ResourceAllocator allocator, Buffer *pBuffer, u64 memSize);
    void AllocateImageMemory(ResourceAllocator allocator, Image *pImage, u64 memSize);

    // * Shaders * //
    Shader *CreateShader(Resource::ShaderResource *pResource);
    void DeleteShader(Shader *pShader);

    // * Descriptor * //

    DescriptorSetLayout *CreateDescriptorSetLayout(
        eastl::span<DescriptorLayoutElement> elements,
        DescriptorSetLayoutFlag flags = DescriptorSetLayoutFlag::DescriptorBuffer);
    u64 GetDescriptorSetLayoutSize(DescriptorSetLayout *pLayout);
    u64 GetDescriptorSetLayoutBindingOffset(DescriptorSetLayout *pLayout, u32 bindingID);
    u64 GetDescriptorSize(DescriptorType type);
    u64 GetDescriptorSizeAligned(DescriptorType type);
    u64 AlignUpDescriptorOffset(u64 offset);
    void GetDescriptorData(const DescriptorGetInfo &info, u64 dataSize, void *pDataOut, u32 descriptorIdx);

    // * Buffers * //
    VkDeviceMemory CreateHeap(u64 heapSize, MemoryFlag flags);

    Buffer *CreateBuffer(BufferDesc *pDesc);
    void DeleteBuffer(Buffer *pHandle, bool waitForWork = true);

    void MapMemory(ResourceAllocator allocator, void *&pData, u64 offset, u64 size);
    void UnmapMemory(ResourceAllocator allocator);
    void MapBuffer(Buffer *pBuffer, void *&pData, u64 offset, u64 size);
    void UnmapBuffer(Buffer *pBuffer);

    // * Images * //
    Image *CreateImage(ImageDesc *pDesc);
    void DeleteImage(Image *pImage, bool waitForWork = true);
    void CreateImageView(Image *pImage, ImageUsage aspectUsage);

    Sampler *CreateSampler(SamplerDesc *pDesc);
    void DeleteSampler(VkSampler pSampler);

    /// UTILITY ///
    template<typename _Type>
    void SetObjectName(_Type *pType, eastl::string_view name)
    {
        static_assert(
            eastl::is_base_of<APIObject<_Type::kObjectType>, _Type>(), "_Type is not base of APIObject");

#if _DEBUG
        VkDebugUtilsObjectNameInfoEXT objectNameInfo = {};
        objectNameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
        objectNameInfo.objectType = _Type::kObjectType;
        objectNameInfo.objectHandle = (u64)pType->m_pHandle;
        objectNameInfo.pObjectName = name.data();

        vkSetDebugUtilsObjectNameEXT(m_pDevice, &objectNameInfo);
#endif
    }

    template<VkObjectType _ObjectType, typename _Type>
    void SetObjectNameRaw(_Type object, eastl::string_view name)
    {
#if _DEBUG
        VkDebugUtilsObjectNameInfoEXT objectNameInfo = {};
        objectNameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
        objectNameInfo.objectType = _ObjectType;
        objectNameInfo.objectHandle = (u64)object;
        objectNameInfo.pObjectName = name.data();

        vkSetDebugUtilsObjectNameEXT(m_pDevice, &objectNameInfo);
#endif
    }

    Semaphore *GetAvailableAcquireSemaphore(Semaphore *pOldSemaphore);

    // * API Instance * //
    bool LoadVulkan();
    bool SetupInstance(BaseWindow *pWindow);
    bool SetupPhysicalDevice();
    bool SetupSurface(BaseWindow *pWindow);
    bool SetupDevice();

    /// API Context
    VkInstance m_pInstance = nullptr;
    PhysicalDevice *m_pPhysicalDevice = nullptr;
    VkDevice m_pDevice = nullptr;

    Surface *m_pSurface = nullptr;
    SwapChain *m_pSwapChain = nullptr;

    /// Main Queues
    CommandQueue *m_pDirectQueue = nullptr;
    CommandQueue *m_pComputeQueue = nullptr;
    CommandQueue *m_pTransferQueue = nullptr;

    /// Pools/Caches
    VkPipelineCache m_pPipelineCache = nullptr;
    eastl::fixed_vector<Semaphore *, 8, false> m_AcquireSempPool;

    ResourceAllocators m_Allocators = {};
    HMODULE m_VulkanLib = NULL;

    TracyVkCtx m_pTracyCtx = nullptr;
};
}  // namespace lr::Graphics