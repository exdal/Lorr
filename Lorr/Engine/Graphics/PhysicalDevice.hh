#pragma once

#include <EASTL/vector.h>

#include "APIObject.hh"
#include "Common.hh"
#include "SwapChain.hh"

namespace lr::Graphics
{
struct PhysicalDevice;
struct Surface : Tracked<VkSurfaceKHR>
{
    APIResult init(PhysicalDevice *physical_device);

    void refresh_surface_capabilities(PhysicalDevice *physical_device);
    bool is_format_supported(VkFormat format, VkColorSpaceKHR &color_space_out);
    bool is_present_mode_supported(VkPresentModeKHR mode);
    VkSurfaceTransformFlagBitsKHR get_transform();

    eastl::vector<VkSurfaceFormatKHR> m_surface_formats = {};
    eastl::vector<VkPresentModeKHR> m_present_modes = {};
    VkSurfaceCapabilitiesKHR m_capabilities = {};
};
LR_ASSIGN_OBJECT_TYPE(Surface, VK_OBJECT_TYPE_SURFACE_KHR);

struct Device;
struct PhysicalDevice : Tracked<VkPhysicalDevice>
{
    APIResult init(VkPhysicalDevice physical_device, DeviceFeature features);

    u32 get_heap_index(VkMemoryPropertyFlags flags);
    u64 get_descriptor_buffer_alignment();
    u64 get_descriptor_size(DescriptorType type);
    u64 get_aligned_buffer_memory(BufferUsage buffer_usage, u64 unaligned_size);

    bool is_feature_supported(DeviceFeature feature);

    VkPhysicalDeviceMemoryProperties m_memory_props = {};
    VkPhysicalDeviceProperties2 m_properties = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
    VkPhysicalDeviceDescriptorBufferPropertiesEXT m_descriptor_buffer_props = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_PROPERTIES_EXT };
    DeviceFeature m_supported_features = {};
};
LR_ASSIGN_OBJECT_TYPE(PhysicalDevice, VK_OBJECT_TYPE_PHYSICAL_DEVICE);

struct DeviceMemoryDesc
{
    MemoryFlag m_flags = MemoryFlag::Device;
    u64 m_size = 0;
};

using DeviceMemoryID = usize;
struct DeviceMemory : Tracked<VkDeviceMemory>
{
    void *m_mapped_memory = nullptr;
};
LR_ASSIGN_OBJECT_TYPE(DeviceMemory, VK_OBJECT_TYPE_DEVICE_MEMORY);

}  // namespace lr::Graphics