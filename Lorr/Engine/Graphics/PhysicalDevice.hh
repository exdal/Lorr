#pragma once

#include "Memory/Allocator/AreaAllocator.hh"

#include <EASTL/vector.h>

#include "APIObject.hh"
#include "Common.hh"
#include "SwapChain.hh"

namespace lr::Graphics
{
struct AvailableQueueInfo
{
    u32 m_present : 1;
    u32 m_queue_count : 15;
    u32 m_index : 16;
};

struct Surface : APIObject
{
    Surface(VkSurfaceKHR handle);

    bool is_format_supported(VkFormat format, VkColorSpaceKHR &color_space_out);
    bool is_present_mode_supported(VkPresentModeKHR mode);
    VkSurfaceTransformFlagBitsKHR get_transform();

    eastl::vector<VkSurfaceFormatKHR> m_surface_formats;
    eastl::vector<VkPresentModeKHR> m_present_modes;

    VkSurfaceCapabilitiesKHR m_capabilities = {};
    VkSurfaceKHR m_handle = VK_NULL_HANDLE;
};
LR_ASSIGN_OBJECT_TYPE(Surface, VK_OBJECT_TYPE_SURFACE_KHR);

struct PhysicalDevicePropertySet
{
    PhysicalDevicePropertySet();

    VkPhysicalDeviceMemoryProperties m_memory = {};

    /// CHAINED ///
    VkPhysicalDeviceProperties2 m_properties = {};
    VkPhysicalDeviceDescriptorBufferPropertiesEXT m_descriptor_buffer = {};
};

struct Device;
struct PhysicalDevice : APIObject
{
    PhysicalDevice(VkPhysicalDevice handle);
    void init_queue_families(Surface *surface);
    Device *get_logical_device();
    u32 get_heap_index(VkMemoryPropertyFlags flags);
    u32 get_queue_index(CommandType type);
    u32 select_queue(Surface *surface, VkQueueFlags desired_queue, bool require_present, bool select_best);

    u64 get_descriptor_buffer_alignment();
    u64 get_descriptor_size(DescriptorType type);

    u64 get_aligned_buffer_memory(BufferUsage buffer_usage, u64 unaligned_size);

    void get_surface_formats(Surface *surface, eastl::vector<VkSurfaceFormatKHR> &formats);
    void get_present_modes(Surface *surface, eastl::vector<VkPresentModeKHR> &modes);
    void set_surface_capabilities(Surface *surface);

    u32 m_present_queue_index = 0;
    eastl::array<VkDeviceQueueCreateInfo, (u32)CommandType::Count> m_queue_infos = {};
    eastl::vector<VkQueueFamilyProperties> m_queue_properties = {};

    PhysicalDevicePropertySet m_property_set = {};
    VkPhysicalDevice m_handle = VK_NULL_HANDLE;
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
    AllocatorType m_type = AllocatorType::Linear;
    MemoryFlag m_flags = MemoryFlag::Device;
    u64 m_size = 0;
    u32 m_max_allocations = 0;
};

struct DeviceMemory : APIObject
{
    virtual ~DeviceMemory() = default;
    virtual void create(u64 mem_size, u32 max_allocations) = 0;
    virtual u64 allocate_memory(u64 data_size, u64 alignment, u64 &allocator_data) = 0;
    virtual void free_memory(u64 allocator_data) = 0;

    void *m_mapped_memory = nullptr;
    VkDeviceMemory m_handle = VK_NULL_HANDLE;
};
LR_ASSIGN_OBJECT_TYPE(DeviceMemory, VK_OBJECT_TYPE_DEVICE_MEMORY);

struct TLSFDeviceMemory : DeviceMemory
{
    void create(u64 mem_size, u32 max_allocations) override;
    u64 allocate_memory(u64 dataSize, u64 alignment, u64 &allocator_data) override;
    void free_memory(u64 allocatorData) override;

    Memory::TLSFAllocatorView m_allocator_view = {};
};

struct LinearDeviceMemory : DeviceMemory
{
    void create(u64 mem_size, u32 max_allocations = 0) override;
    u64 allocate_memory(u64 dataSize, u64 alignment, u64 &allocator_data) override;
    void free_memory(u64 allocatorData) override;

    Memory::AreaAllocatorView m_allocator_view = {};
};

}  // namespace lr::Graphics