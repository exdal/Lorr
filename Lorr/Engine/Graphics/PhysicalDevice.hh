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

enum class AllocatorType : u32
{
    Linear = 0,
    TLSF,
};

enum class MemoryFlag : u32
{
    Device = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    HostVisible = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
    HostCoherent = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
    HostCached = VK_MEMORY_PROPERTY_HOST_CACHED_BIT,

    HostVisibleCoherent = HostVisible | HostCoherent,
};
LR_TYPEOP_ARITHMETIC_INT(MemoryFlag, MemoryFlag, &);
LR_TYPEOP_ARITHMETIC(MemoryFlag, MemoryFlag, |);

struct DeviceMemoryDesc
{
    AllocatorType m_Type = AllocatorType::Linear;
    MemoryFlag m_Flags = MemoryFlag::Device;
    u64 m_Size = 0;
};

struct DeviceMemory : APIObject
{
    u64 AlignBufferMemory(BufferUsage usage, u64 initialSize);
    virtual void Init(u64 memSize) = 0;
    virtual u64 Allocate(u64 dataSize, u64 alignment, u64 &allocatorData) = 0;
    virtual void Free(u64 allocatorData) = 0;

    void *m_pMappedMemory = nullptr;
    PhysicalDevice *m_pPhysicalDevice = nullptr;  // Device where it's allocated from
    VkDeviceMemory m_pHandle = VK_NULL_HANDLE;
};
LR_ASSIGN_OBJECT_TYPE(DeviceMemory, VK_OBJECT_TYPE_DEVICE_MEMORY);

struct TLSFDeviceMemory : DeviceMemory
{
    void Init(u64 memSize) override;
    u64 Allocate(u64 dataSize, u64 alignment, u64 &allocatorData) override;
    void Free(u64 allocatorData) override;

    Memory::TLSFAllocatorView m_Allocator = {};
};

struct LinearDeviceMemory : DeviceMemory
{
    void Init(u64 memSize) override;
    u64 Allocate(u64 dataSize, u64 alignment, u64 &allocatorData) override;
    void Free(u64 allocatorData) override;

    Memory::LinearAllocatorView m_AllocatorView = {};
};

}  // namespace lr::Graphics