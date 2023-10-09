#pragma once

#include "Memory/Allocator/LinearAllocator.hh"
#include "Memory/MemoryUtils.hh"
#include "Window/BaseWindow.hh"

#include <EASTL/vector.h>

#include "APIObject.hh"
#include "Common.hh"

namespace lr::Graphics
{
struct AvailableQueueInfo
{
    u32 m_Present : 1;
    u32 m_QueueCount : 15;
    u32 m_Index : 16;
};

struct Surface;
struct PhysicalDevice : APIObject
{
    void Init(eastl::span<VkPhysicalDevice> devices);
    void InitQueueFamilies(Surface *pSurface);
    VkDevice GetLogicalDevice();
    u32 GetHeapIndex(VkMemoryPropertyFlags flags);
    u32 GetQueueIndex(CommandType type);
    u32 SelectQueue(
        Surface *pSurface,
        VkQueueFlags desiredQueue,
        bool requirePresent,
        bool selectBest);

    u64 GetDescriptorBufferAlignment();
    u64 GetDescriptorSize(DescriptorType type);

    u32 m_PresentQueueIndex = 0;
    eastl::array<VkDeviceQueueCreateInfo, (u32)CommandType::Count> m_QueueInfos;
    eastl::vector<VkQueueFamilyProperties> m_QueueProperties;
    eastl::vector<u32> m_SelectedQueueIndices;
    eastl::vector<VkExtensionProperties> m_DeviceExtensions;

    /// DEVICE FEATURES ///
    VkPhysicalDeviceProperties m_FeatureDeviceProps = {};
    VkPhysicalDeviceMemoryProperties m_FeatureMemoryProps = {};
    // VK_EXT_descriptor_buffer
    VkPhysicalDeviceDescriptorBufferPropertiesEXT m_FeatureDescriptorBufferProps = {};

    VkPhysicalDevice m_pHandle = VK_NULL_HANDLE;
};
LR_ASSIGN_OBJECT_TYPE(PhysicalDevice, VK_OBJECT_TYPE_PHYSICAL_DEVICE);

struct Surface : APIObject
{
    void Init(BaseWindow *pWindow, VkInstance pInstance, PhysicalDevice *pPhysicalDevice);
    bool IsFormatSupported(VkFormat format, VkColorSpaceKHR &colorSpaceOut);
    bool IsPresentModeSupported(VkPresentModeKHR mode);

    eastl::vector<VkSurfaceFormatKHR> m_SurfaceFormats;
    eastl::vector<VkPresentModeKHR> m_PresentModes;

    VkCompositeAlphaFlagsKHR m_SupportedCompositeAlpha =
        VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    VkSurfaceTransformFlagsKHR m_SupportedTransforms =
        VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    VkSurfaceTransformFlagBitsKHR m_CurrentTransform =
        VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;

    u32 m_MinImageCount = 0;
    u32 m_MaxImageCount = 0;
    XMUINT2 m_MinImageSize = {};
    XMUINT2 m_MaxImageSize = {};

    VkSurfaceKHR m_pHandle = VK_NULL_HANDLE;
};
LR_ASSIGN_OBJECT_TYPE(Surface, VK_OBJECT_TYPE_SURFACE_KHR);

struct DeviceMemory : APIObject
{
    virtual u64 Allocate(u64 dataSize, u64 alignment, u64 &allocatorData) = 0;
    virtual void Free(u64 allocatorData) = 0;

    void *m_pMappedMemory = nullptr;
    VkDeviceMemory m_pHandle = VK_NULL_HANDLE;
};
LR_ASSIGN_OBJECT_TYPE(DeviceMemory, VK_OBJECT_TYPE_DEVICE_MEMORY);

struct TLSFDeviceMemory : DeviceMemory
{
    u64 Allocate(u64 dataSize, u64 alignment, u64 &allocatorData) override
    {
        ZoneScoped;

        Memory::TLSFBlockID blockID = m_Allocator.Allocate(dataSize, alignment);
        allocatorData = blockID;
        return m_Allocator.GetBlockData(blockID)->m_Offset;
    }

    void Free(u64 allocatorData) override
    {
        ZoneScoped;

        m_Allocator.Free(allocatorData);
    }

    Memory::TLSFAllocatorView m_Allocator = {};
};

struct LinearDeviceMemory : DeviceMemory
{
    void Init(u64 memSize)
    {
        ZoneScoped;

        m_AllocatorView.m_pFirstRegion = new Memory::AllocationRegion;
        m_AllocatorView.m_pFirstRegion->m_Capacity = memSize;
    }

    u64 Allocate(u64 dataSize, u64 alignment, u64 &allocatorData) override
    {
        ZoneScoped;

        u64 alignedSize = Memory::AlignUp(dataSize, alignment);
        Memory::AllocationRegion *pAvailRegion =
            m_AllocatorView.FindFreeRegion(alignedSize);
        if (!pAvailRegion)
            return ~0;

        u64 offset = pAvailRegion->m_Size;
        pAvailRegion->m_Size += alignedSize;
        return offset;
    }

    void Free(u64 allocatorData) override
    {
        ZoneScoped;

        m_AllocatorView.Reset();
    }

    Memory::LinearAllocatorView m_AllocatorView = {};
};

}  // namespace lr::Graphics