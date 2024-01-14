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
    APIResult init(VkPhysicalDevice handle, DeviceFeature features);


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
    MemoryFlag m_flags = {};
    void *m_mapped_memory = nullptr;
};
LR_ASSIGN_OBJECT_TYPE(DeviceMemory, VK_OBJECT_TYPE_DEVICE_MEMORY);

}  // namespace lr::Graphics