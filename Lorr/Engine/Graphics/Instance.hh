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
    eastl::string_view m_app_name = {};
    u32 m_app_version = {};
    eastl::string_view m_engine_name = {};
    u32 m_engine_version = {};
    u32 m_api_version = {};
};

struct PhysicalDevice;
struct Surface;
// No APIObject for Instance, it's the head of all Graphics class
struct Instance
{
    bool create(InstanceDesc *desc);
    PhysicalDevice *get_physical_device();
    bool check_physical_device_extensions(VkPhysicalDevice physical_device, eastl::span<const char *> required_extensions);
    Surface *get_win32_surface(Win32Window *window);

    void *m_vulkan_lib = nullptr;
    VkInstance m_handle = nullptr;
};
LR_ASSIGN_OBJECT_TYPE(Instance, VK_OBJECT_TYPE_INSTANCE);

}  // namespace lr::Graphics