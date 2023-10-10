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
    eastl::string_view m_AppName;
    u32 m_AppVersion;
    eastl::string_view m_EngineName;
    u32 m_EngineVersion;
    u32 m_APIVersion;
};

struct PhysicalDevice;
struct Surface;
// No APIObject for Instance, it's the head of all Graphics class
struct Instance
{
    bool Init(InstanceDesc *pDesc);
    PhysicalDevice *GetPhysicalDevice();
    Surface *GetWin32Surface(Win32Window *pWindow);

    void *m_pVulkanLib = nullptr;
    VkInstance m_pHandle = nullptr;
};
LR_ASSIGN_OBJECT_TYPE(Instance, VK_OBJECT_TYPE_INSTANCE);

}  // namespace lr::Graphics