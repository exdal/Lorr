//
// Created on Tuesday 10th May 2022 by exdal
//

#pragma once

#include "IO/BufferStream.hh"
#include "Memory/Allocator/LinearAllocator.hh"
#include "Memory/Allocator/TLSFAllocator.hh"

#include "Device.hh"
#include "CommandQueue.hh"
#include "CommandList.hh"
#include "Pipeline.hh"
#include "SwapChain.hh"

namespace lr::Graphics
{
enum APIFlags : u32
{
    LR_API_FLAG_NONE = 0,
    LR_API_FLAG_VSYNC = 1 << 0,
};
EnumFlags(APIFlags);

struct APIAllocatorInitDesc
{
    u32 m_MaxTLSFAllocations = 0x2000;
    u32 m_DescriptorMem = Memory::MiBToBytes(1);
    u32 m_BufferLinearMem = Memory::MiBToBytes(64);
    u32 m_BufferTLSFMem = Memory::MiBToBytes(1024);
    u32 m_BufferTLSFHostMem = Memory::MiBToBytes(128);
    u32 m_BufferFrametimeMem = Memory::MiBToBytes(128);
    u32 m_ImageTLSFMem = Memory::MiBToBytes(1024);
};

struct APIDesc
{
    APIFlags m_Flags = LR_API_FLAG_NONE;
    SwapChainFlag m_SwapChainFlags = SwapChainFlag::None;
    BaseWindow *m_pTargetWindow = nullptr;
    APIAllocatorInitDesc m_AllocatorDesc = {};
};

struct SubmitDesc
{
    CommandType m_Type = CommandType::Count;
    eastl::span<SemaphoreSubmitDesc> m_WaitSemas;
    eastl::span<CommandListSubmitDesc> m_Lists;
    eastl::span<SemaphoreSubmitDesc> m_SignalSemas;
};

struct Context
{
    bool Init(APIDesc *pDesc);
    void InitAllocators(APIAllocatorInitDesc *pDesc);

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
    VkPipelineLayout SerializePipelineLayout(PipelineLayoutSerializeDesc *pDesc, Pipeline *pPipeline);

    Pipeline *CreateGraphicsPipeline(GraphicsPipelineBuildInfo *pBuildInfo);
    Pipeline *CreateComputePipeline(ComputePipelineBuildInfo *pBuildInfo);

    /// SWAPCHAIN ///
    SwapChain *CreateSwapChain(BaseWindow *pWindow, SwapChainFlag flags, SwapChain *pOldSwapChain);
    void DeleteSwapChain(SwapChain *pSwapChain, bool keepSelf);
    void ResizeSwapChain(BaseWindow *pWindow);
    SwapChain *GetSwapChain();

    void WaitForWork();
    void BeginFrame();
    void EndFrame();

    /// RESOURCE ///
    // * Shaders * //
    Shader *CreateShader(ShaderStage stage, BufferReadStream &buf);
    Shader *CreateShader(ShaderStage stage, eastl::string_view path);
    void DeleteShader(Shader *pShader);

    // * Descriptor * //

    DescriptorSetLayout *CreateDescriptorSetLayout(
        eastl::span<DescriptorLayoutElement> elements,
        DescriptorSetLayoutType type = DescriptorSetLayoutType::None);
    u64 GetDescriptorSetLayoutSize(DescriptorSetLayout *pLayout);  // ALIGNED!!!
    u64 GetDescriptorSetLayoutBindingOffset(DescriptorSetLayout *pLayout, u32 bindingID);
    void GetDescriptor();

    // * Buffers * //
    VkDeviceMemory CreateHeap(u64 heapSize, bool cpuWrite);

    Buffer *CreateBuffer(BufferDesc *pDesc);
    void DeleteBuffer(Buffer *pHandle, bool waitForWork = true);

    void MapMemory(Buffer *pBuffer, void *&pData);
    void UnmapMemory(Buffer *pBuffer);

    // * Images * //
    Image *CreateImage(ImageDesc *pDesc);
    void DeleteImage(Image *pImage, bool waitForWork = true);
    void CreateImageView(Image *pImage);

    Sampler *CreateSampler(SamplerDesc *pDesc);
    void DeleteSampler(VkSampler pSampler);

    void SetAllocator(Buffer *pBuffer, ResourceAllocator targetAllocator);
    void SetAllocator(Image *pImage, ResourceAllocator targetAllocator);

    /// UTILITY ///
    template<typename _Type>
    void SetObjectName(_Type *pType, eastl::string_view name)
    {
#if _DEBUG
        VkDebugUtilsObjectNameInfoEXT objectNameInfo = {};
        objectNameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
        objectNameInfo.objectType = _Type::kObjectType;
        objectNameInfo.objectHandle = (u64)pType->m_pHandle;
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

    /// ----------------------------------------------------------- ///

    template<typename _Allocator>
    struct BufferedAllocator
    {
        _Allocator Allocator;
        VkDeviceMemory pHeap;
    };

    /// ----------------------------------------------------------- ///

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

    using VKLinearAllocator = BufferedAllocator<Memory::LinearAllocatorView>;
    using VKTLSFAllocator = BufferedAllocator<Memory::TLSFAllocatorView>;

    VKLinearAllocator m_MADescriptor;
    VKLinearAllocator m_MABufferLinear;
    VKLinearAllocator m_MABufferFrametime[LR_MAX_FRAME_COUNT] = {};

    VKTLSFAllocator m_MABufferTLSF;
    VKTLSFAllocator m_MABufferTLSFHost;
    VKTLSFAllocator m_MAImageTLSF;

    HMODULE m_VulkanLib;
};

}  // namespace lr::Graphics