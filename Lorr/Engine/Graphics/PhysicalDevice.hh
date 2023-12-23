#pragma once

#include <EASTL/vector.h>

#include "APIObject.hh"
#include "Common.hh"
#include "SwapChain.hh"

namespace lr::Graphics
{
struct PhysicalDevice;
struct Surface
{
    APIResult init(PhysicalDevice *physical_device);

    void refresh_surface_capabilities(PhysicalDevice *physical_device);
    bool is_format_supported(VkFormat format, VkColorSpaceKHR &color_space_out);
    bool is_present_mode_supported(VkPresentModeKHR mode);
    VkSurfaceTransformFlagBitsKHR get_transform();

    eastl::vector<VkSurfaceFormatKHR> m_surface_formats = {};
    eastl::vector<VkPresentModeKHR> m_present_modes = {};
    VkSurfaceCapabilitiesKHR m_capabilities = {};

    VkSurfaceKHR m_handle = VK_NULL_HANDLE;
    explicit operator bool() { return m_handle != nullptr; }
    operator VkSurfaceKHR &() { return m_handle; }
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
struct PhysicalDevice
{
    APIResult init(VkPhysicalDevice physical_device, eastl::span<QueueFamilyInfo> queue_family_infos);

    APIResult get_logical_device(Device *device);
    u32 get_heap_index(VkMemoryPropertyFlags flags);

    u64 get_descriptor_buffer_alignment();
    u64 get_descriptor_size(DescriptorType type);

    u64 get_aligned_buffer_memory(BufferUsage buffer_usage, u64 unaligned_size);

    static eastl::span<const char *> get_extensions();

    eastl::vector<QueueFamilyInfo> m_queue_family_infos = {};
    PhysicalDevicePropertySet m_property_set = {};

    VkPhysicalDevice m_handle = VK_NULL_HANDLE;
    operator VkPhysicalDevice &() { return m_handle; }
    explicit operator bool() { return m_handle != nullptr; }
};
LR_ASSIGN_OBJECT_TYPE(PhysicalDevice, VK_OBJECT_TYPE_PHYSICAL_DEVICE);

struct DeviceMemoryDesc
{
    MemoryFlag m_flags = MemoryFlag::Device;
    u64 m_size = 0;
};

using DeviceMemoryID = usize;
struct DeviceMemory
{
    void *m_mapped_memory = nullptr;

    VkDeviceMemory m_handle = VK_NULL_HANDLE;
    explicit operator bool() { return m_handle != nullptr; }
    operator VkDeviceMemory &() { return m_handle; }
};
LR_ASSIGN_OBJECT_TYPE(DeviceMemory, VK_OBJECT_TYPE_DEVICE_MEMORY);

}  // namespace lr::Graphics