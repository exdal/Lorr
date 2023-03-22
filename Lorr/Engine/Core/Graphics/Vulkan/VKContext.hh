//
// Created on Tuesday 10th May 2022 by exdal
//

#pragma once

#include "Core/IO/BufferStream.hh"
#include "Core/Memory/Allocator/LinearAllocator.hh"
#include "Core/Memory/Allocator/TLSFAllocator.hh"

#include "Vulkan.hh"

#include "VKDevice.hh"
#include "VKCommandQueue.hh"
#include "VKCommandList.hh"
#include "VKPipeline.hh"
#include "VKSwapChain.hh"

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
    SwapChainFlags m_SwapChainFlags = LR_SWAP_CHAIN_FLAG_NONE;
    BaseWindow *m_pTargetWindow = nullptr;
    APIAllocatorInitDesc m_AllocatorDesc = {};
};

struct SubmitDesc
{
    CommandListType m_Type = LR_COMMAND_LIST_TYPE_COUNT;
    eastl::span<SemaphoreSubmitDesc> m_WaitSemas;
    eastl::span<CommandListSubmitDesc> m_Lists;
    eastl::span<SemaphoreSubmitDesc> m_SignalSemas;
};

struct VKContext
{
    bool Init(APIDesc *pDesc);
    void InitAllocators(APIAllocatorInitDesc *pDesc);

    /// COMMAND ///
    CommandQueue *CreateCommandQueue(CommandListType type);
    CommandAllocator *CreateCommandAllocator(CommandListType type, bool resetLists);
    CommandList *CreateCommandList(CommandListType type, CommandAllocator *pAllocator = nullptr);

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
    VKSwapChain *CreateSwapChain(BaseWindow *pWindow, SwapChainFlags flags, VKSwapChain *pOldSwapChain);
    void DeleteSwapChain(VKSwapChain *pSwapChain, bool keepSelf);
    void AdvanceSwapChain();
    void ResizeSwapChain(BaseWindow *pWindow);
    VKSwapChain *GetSwapChain();

    void WaitForWork();
    void BeginFrame();
    void EndFrame();

    /// RESOURCE ///
    // * Shaders * //
    Shader *CreateShader(ShaderStage stage, BufferReadStream &buf);
    Shader *CreateShader(ShaderStage stage, eastl::string_view path);
    void DeleteShader(Shader *pShader);

    DescriptorSetLayout *CreateDescriptorSetLayout(eastl::span<DescriptorLayoutElement> elements);

    DescriptorSet *CreateDescriptorSet(DescriptorSetLayout *pLayout);
    void DeleteDescriptorSet(DescriptorSet *pSet);
    void UpdateDescriptorSet(DescriptorSet *pSet, cinitl<DescriptorWriteData> &elements);

    VkDescriptorPool CreateDescriptorPool(cinitl<DescriptorPoolDesc> &layouts);

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

    void SetAllocator(Buffer *pBuffer, APIAllocatorType targetAllocator);
    void SetAllocator(Image *pImage, APIAllocatorType targetAllocator);

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
    bool SetupQueues(VkPhysicalDevice &pPhysicalDevice, VkPhysicalDeviceFeatures &features);

    /// ------------------------------------------------------------- ///

    // clang-format off

    /// Type conversions
    static VkFormat                 ToVKFormat(ImageFormat format);
    static VkFormat                 ToVKFormat(VertexAttribType format);
    static VkPrimitiveTopology      ToVKTopology(PrimitiveType type);
    static VkCullModeFlags          ToVKCullMode(CullMode mode);
    static VkDescriptorType         ToVKDescriptorType(DescriptorType type);
    static VkImageUsageFlags        ToVKImageUsage(ImageUsage usage);
    static VkBufferUsageFlagBits    ToVKBufferUsage(BufferUsage usage);
    static VkImageLayout            ToVKImageLayout(ImageLayout layout);
    static VkShaderStageFlagBits    ToVKShaderType(ShaderStage type);
    static VkPipelineStageFlags2    ToVKPipelineStage(PipelineStage stage);
    static VkAccessFlags2           ToVKAccessFlags(MemoryAccess access);

    // clang-format on

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
    VKPhysicalDevice *m_pPhysicalDevice = nullptr;
    VkDevice m_pDevice = nullptr;

    VKSurface *m_pSurface = nullptr;
    VKSwapChain *m_pSwapChain = nullptr;

    /// Main Queues
    CommandQueue *m_pDirectQueue = nullptr;
    CommandQueue *m_pComputeQueue = nullptr;
    CommandQueue *m_pTransferQueue = nullptr;

    /// Pools/Caches
    VkPipelineCache m_pPipelineCache = nullptr;
    VkDescriptorPool m_pDescriptorPool = nullptr;
    eastl::fixed_vector<Semaphore *, 8, false> m_AcquireSempPool;
    DescriptorLayoutCache m_LayoutCache = {};

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