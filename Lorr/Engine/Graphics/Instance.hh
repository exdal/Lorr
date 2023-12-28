#pragma once

#include "APIObject.hh"
#include "Common.hh"

namespace lr
{
struct Win32Window;
}

namespace lr::Graphics
{
// There are no option for layers. Because it's far better to use
// Vulkan Configurator to manage layers. Please use it.
struct InstanceDesc
{
    Win32Window *m_window = nullptr;
    eastl::string_view m_app_name = {};
    u32 m_app_version = {};
    eastl::string_view m_engine_name = {};
    u32 m_engine_version = {};
    u32 m_api_version = {};
};

struct Surface;
struct PhysicalDevice;
struct Device;
struct Instance : Tracked<VkInstance>
{
    APIResult init(InstanceDesc *desc);
    APIResult get_win32_surface(Surface *surface);
    APIResult get_physical_device(PhysicalDevice *physical_device);
    APIResult get_logical_device(Device *device, PhysicalDevice *physical_device);

    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    VkPhysicalDevice m_physical_device = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    DeviceFeature m_supported_features = {};

    u32 m_present_queue_index = 0;
    eastl::array<u32, (u32)CommandType::Count> m_queue_indexes = {};

    void *m_vulkan_lib = nullptr;
};
LR_ASSIGN_OBJECT_TYPE(Instance, VK_OBJECT_TYPE_INSTANCE);

}  // namespace lr::Graphics