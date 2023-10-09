#pragma once

#include "APIObject.hh"
#include "Common.hh"

namespace lr::Graphics
{
// There are no option for layers. Because it's far better to use
// Vulkan Configurator to manage layers. Please use it.
struct InstanceDesc
{
    eastl::span<const char *> m_Extensions;
    eastl::string_view m_AppName;
    u32 m_AppVersion;
    eastl::string_view m_EngineName;
    u32 m_EngineVersion;
    u32 m_APIVersion;
};

struct Instance : APIObject
{
    bool Init(InstanceDesc *pDesc);

    VkInstance m_pInstance = nullptr;
};
LR_ASSIGN_OBJECT_TYPE(Instance, VK_OBJECT_TYPE_INSTANCE);

}  // namespace lr::Graphics