#pragma once

#include "APIObject.hh"
#include "Common.hh"

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

struct Device;
struct Instance
{
    APIResult init(const InstanceDesc &desc);
    APIResult create_devce(Device *device);

    VkInstance m_handle = nullptr;
};
LR_ASSIGN_OBJECT_TYPE(Instance, VK_OBJECT_TYPE_INSTANCE);

}  // namespace lr::Graphics
