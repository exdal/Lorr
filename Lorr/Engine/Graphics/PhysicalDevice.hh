#pragma once

#include "Memory/Allocator/LinearAllocator.hh"
#include "Memory/MemoryUtils.hh"
#include "Window/BaseWindow.hh"

#include <EASTL/vector.h>

#include "APIObject.hh"
#include "Common.hh"
#include "SwapChain.hh"

namespace lr::Graphics
{
struct AvailableQueueInfo
{
    u32 m_Present : 1;
    u32 m_QueueCount : 15;
    u32 m_Index : 16;
};

struct Surface : APIObject
{
    bool Init(VkSurfaceKHR pHandle);
    bool IsFormatSupported(VkFormat format, VkColorSpaceKHR &colorSpaceOut);
    bool IsPresentModeSupported(VkPresentModeKHR mode);
    VkSurfaceTransformFlagBitsKHR GetTransform();

    eastl::vector<VkSurfaceFormatKHR> m_SurfaceFormats;
    eastl::vector<VkPresentModeKHR> m_PresentModes;

    VkSurfaceCapabilitiesKHR m_Capabilities = {};
    VkSurfaceKHR m_pHandle = VK_NULL_HANDLE;
};
LR_ASSIGN_OBJECT_TYPE(Surface, VK_OBJECT_TYPE_SURFACE_KHR);

struct PhysicalDevicePropertySet
{
    PhysicalDevicePropertySet();
    
    VkPhysicalDeviceMemoryProperties m_Memory = {};

    /// CHAINED ///
    VkPhysicalDeviceProperties2 m_Properties = {};
    VkPhysicalDeviceDescriptorBufferPropertiesEXT m_DescriptorBuffer = {};
};

struct Device;
struct PhysicalDevice : APIObject
{
    bool Init(VkPhysicalDevice pDevice);
    void InitQueueFamilies(Surface *pSurface);
    Device *GetLogicalDevice();
    u32 GetHeapIndex(VkMemoryPropertyFlags flags);
    u32 GetQueueIndex(CommandType type);
    u32 SelectQueue(
        Surface *pSurface,
        VkQueueFlags desiredQueue,
        bool requirePresent,
        bool selectBest);

    u64 GetDescriptorBufferAlignment();
    u64 GetDescriptorSize(DescriptorType type);

    void GetSurfaceFormats(Surface *pSurface, eastl::vector<VkSurfaceFormatKHR> &formats);
    void GetPresentModes(Surface *pSurface, eastl::vector<VkPresentModeKHR> &modes);
    void SetSurfaceCapabilities(Surface *pSurface);

    u32 m_PresentQueueIndex = 0;
    eastl::array<VkDeviceQueueCreateInfo, (u32)CommandType::Count> m_QueueInfos;
    eastl::vector<VkQueueFamilyProperties> m_QueueProperties;

    PhysicalDevicePropertySet m_PropertySet = {};
    VkPhysicalDevice m_pHandle = VK_NULL_HANDLE;
};
LR_ASSIGN_OBJECT_TYPE(PhysicalDevice, VK_OBJECT_TYPE_PHYSICAL_DEVICE);

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

    virtual ~DeviceMemory() = default;
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