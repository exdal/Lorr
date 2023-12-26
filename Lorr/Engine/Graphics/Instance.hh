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

enum class QueueSelectType : u64
{
    DoNotSelect = 0,
    PreferDedicated,
    RequireDedicated,
};

struct PhysicalDeviceSelectInfo
{
    eastl::array<QueueSelectType, (u32)CommandType::Count> m_queue_select_types = {};
    eastl::span<const char *> m_required_extensions = {};
};

struct PhysicalDevice;
struct Surface;
struct Instance : Tracked<VkInstance>
{
    APIResult init(InstanceDesc *desc);
    APIResult select_physical_device(PhysicalDevice *physical_device, PhysicalDeviceSelectInfo *select_info);
    APIResult get_win32_surface(Surface *surface, Win32Window *window);

    eastl::vector<VkPhysicalDevice> m_physical_devices = {};

    void *m_vulkan_lib = nullptr;
};
LR_ASSIGN_OBJECT_TYPE(Instance, VK_OBJECT_TYPE_INSTANCE);

}  // namespace lr::Graphics