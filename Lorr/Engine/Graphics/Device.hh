// Created on Monday July 18th 2022 by exdal
// Last modified on Monday August 28th 2023 by exdal

#pragma once

#include <EASTL/fixed_vector.h>

#include "APIObject.hh"
#include "Resource.hh"
#include "PhysicalDevice.hh"
#include "CommandList.hh"
#include "Pipeline.hh"
#include "SwapChain.hh"
#include "TracyVK.hh"

namespace lr::Graphics
{
enum class APIFlag : u32
{
    None = 0,
    VSync = 1 << 0,
};
LR_TYPEOP_ARITHMETIC_INT(APIFlag, APIFlag, &);

struct SubmitDesc
{
    CommandType m_Type = CommandType::Count;
    eastl::span<SemaphoreSubmitDesc> m_WaitSemas;
    eastl::span<CommandListSubmitDesc> m_Lists;
    eastl::span<SemaphoreSubmitDesc> m_SignalSemas;
};

struct DeviceDesc
{
    APIFlag m_Flags = APIFlag::None;
    u32 m_ImageCount = 1;
    BaseWindow *m_pTargetWindow = nullptr;
};

struct Device
{
    bool Init(DeviceDesc *pDesc);

    /// COMMAND ///
    CommandQueue *CreateCommandQueue(CommandType type, u32 queueIndex);
    CommandAllocator *CreateCommandAllocator(CommandQueue *pQueue, bool resetLists);
    CommandList *CreateCommandList(CommandAllocator *pAllocator);

    void BeginCommandList(CommandList *pList);
    void EndCommandList(CommandList *pList);
    void ResetCommandAllocator(CommandAllocator *pAllocator);
    void Submit(CommandQueue *pQueue, SubmitDesc *pDesc);

    Fence CreateFence(bool signaled);
    void DeleteFence(Fence pFence);
    bool IsFenceSignaled(Fence pFence);
    void ResetFence(Fence pFence);

    Semaphore *CreateSemaphore(u32 initialValue = 0, bool binary = true);
    void DeleteSemaphore(Semaphore *pSemaphore);
    void WaitForSemaphore(
        Semaphore *pSemaphore, u64 desiredValue, u64 timeout = UINT64_MAX);

    /// PIPELINE ///
    VkPipelineCache CreatePipelineCache(
        u32 initialDataSize = 0, void *pInitialData = nullptr);
    PipelineLayout CreatePipelineLayout(
        eastl::span<DescriptorSetLayout> layouts,
        eastl::span<PushConstantDesc> pushConstants);

    Pipeline *CreateGraphicsPipeline(GraphicsPipelineBuildInfo *pBuildInfo);
    Pipeline *CreateComputePipeline(ComputePipelineBuildInfo *pBuildInfo);

    /// SWAPCHAIN ///
    SwapChain *CreateSwapChain(SwapChainDesc *pDesc);
    void DeleteSwapChain(SwapChain *pSwapChain, bool keepSelf);

    void WaitForWork();
    u32 AcquireImage(SwapChain *pSwapChain, Semaphore *pSemaphore);
    void Present(SwapChain *pSwapChain, Semaphore *pSemaphore, CommandQueue *pQueue);

    /// RESOURCE ///
    u64 GetBufferMemorySize(Buffer *pBuffer, u64 *pAlignmentOut = nullptr);
    u64 GetImageMemorySize(Image *pImage, u64 *pAlignmentOut = nullptr);

    DeviceMemory *CreateDeviceMemory(
        DeviceMemoryDesc *pDesc, PhysicalDevice *pPhysicalDevice);
    void DeleteDeviceMemory(DeviceMemory *pDeviceMemory);
    void AllocateBufferMemory(DeviceMemory *pMemory, Buffer *pBuffer, u64 memorySize);
    void AllocateImageMemory(DeviceMemory *pMemory, Image *pImage, u64 memorySize);

    // * Shaders * //
    Shader *CreateShader(ShaderStage stage, u32 *pData, u64 dataSize);
    void DeleteShader(Shader *pShader);

    // * Descriptor * //

    DescriptorSetLayout CreateDescriptorSetLayout(
        eastl::span<DescriptorLayoutElement> elements,
        DescriptorSetLayoutFlag flags = DescriptorSetLayoutFlag::DescriptorBuffer);
    void DeleteDescriptorSetLayout(DescriptorSetLayout pLayout);
    u64 GetDescriptorSetLayoutSize(DescriptorSetLayout pLayout);
    u64 GetDescriptorSetLayoutBindingOffset(DescriptorSetLayout pLayout, u32 bindingID);
    void GetDescriptorData(const DescriptorGetInfo &info, u64 dataSize, void *pDataOut);

    // * Buffers * //
    Buffer *CreateBuffer(BufferDesc *pDesc, DeviceMemory *pAllocator);
    void DeleteBuffer(Buffer *pHandle, DeviceMemory *pAllocator);

    u8 *GetMemoryData(DeviceMemory *pMemory);
    u8 *GetBufferMemoryData(DeviceMemory *pMemory, Buffer *pBuffer);

    // * Images * //
    Image *CreateImage(ImageDesc *pDesc, DeviceMemory *pAllocator);
    void DeleteImage(Image *pImage, DeviceMemory *pAllocator);
    void CreateImageView(Image *pImage, ImageUsage aspectUsage);

    Sampler *CreateSampler(SamplerDesc *pDesc);
    void DeleteSampler(VkSampler pSampler);

    /// UTILITY ///
    template<typename _Type>
    void SetObjectName(_Type *pType, eastl::string_view name)
    {
        static_assert(
            eastl::is_base_of<APIObject<_Type::kObjectType>, _Type>(),
            "_Type is not base of APIObject");

#if LR_DEBUG
        VkDebugUtilsObjectNameInfoEXT objectNameInfo = {};
        objectNameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
        objectNameInfo.objectType = (VkObjectType)_Type::kObjectType;
        objectNameInfo.objectHandle = (u64)pType->m_pHandle;
        objectNameInfo.pObjectName = name.data();

        vkSetDebugUtilsObjectNameEXT(m_pHandle, &objectNameInfo);
#endif
    }

    template<VkObjectType _ObjectType, typename _Type>
    void SetObjectNameRaw(_Type object, eastl::string_view name)
    {
#if LR_DEBUG
        VkDebugUtilsObjectNameInfoEXT objectNameInfo = {};
        objectNameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
        objectNameInfo.objectType = _ObjectType;
        objectNameInfo.objectHandle = (u64)object;
        objectNameInfo.pObjectName = name.data();

        vkSetDebugUtilsObjectNameEXT(m_pHandle, &objectNameInfo);
#endif
    }

    VkDevice m_pHandle = nullptr;

    /// Pools/Caches
    VkPipelineCache m_pPipelineCache = nullptr;
    TracyVkCtx m_pTracyCtx = nullptr;
};

}  // namespace lr::Graphics