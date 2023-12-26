#include "Vulkan.hh"

#include "Core/FileSystem.hh"

/// DEFINE VULKAN FUNCTIONS
#define _VK_DEFINE_FUNCTION(_name) PFN_##_name _name
_VK_FUNCTION_NAMES
_VK_FUNCTION_NAMES_DEVICE
_VK_FUNCTION_NAMES_INSTANCE
_VK_FUNCTION_NAMES_INSTANCE_DEBUG
#undef _VK_DEFINE_FUNCTION

namespace lr::Graphics
{
void *VK::LoadVulkan()
{
    ZoneScoped;

    auto vulkan_lib = fs::load_lib("vulkan-1.dll");

    if (vulkan_lib == nullptr)
    {
        LOG_CRITICAL("Failed to find Vulkan dynamic library.");
        fs::free_lib(vulkan_lib);
        return nullptr;
    }

#define _VK_DEFINE_FUNCTION(_name)                                 \
    _name = (PFN_##_name)fs::get_lib_func(vulkan_lib, #_name);     \
    if (_name == nullptr)                                          \
    {                                                              \
        LOG_CRITICAL("Cannot load Vulkan function '{}'!", #_name); \
        fs::free_lib(vulkan_lib);                                  \
        return nullptr;                                            \
    }

    _VK_FUNCTION_NAMES
#undef _VK_DEFINE_FUNCTION

    return vulkan_lib;
}

bool VK::LoadVulkanInstance(VkInstance pInstance)
{
    ZoneScoped;

#define _VK_DEFINE_FUNCTION(_name)                                          \
    _name = (PFN_##_name)vkGetInstanceProcAddr(pInstance, #_name);          \
    if (_name == nullptr)                                                   \
    {                                                                       \
        LOG_CRITICAL("Cannot load Vulkan Instance function '{}'!", #_name); \
        return false;                                                       \
    }
#if _DEBUG
    _VK_FUNCTION_NAMES_INSTANCE_DEBUG
#endif
    _VK_FUNCTION_NAMES_INSTANCE
#undef _VK_DEFINE_FUNCTION

    return true;
}

bool VK::LoadVulkanDevice(VkDevice pDevice)
{
    ZoneScoped;

#define _VK_DEFINE_FUNCTION(_name)                                    \
    _name = (PFN_##_name)vkGetDeviceProcAddr(pDevice, #_name);        \
    if (_name == nullptr)                                             \
    {                                                                 \
        LOG_WARN("Cannot load Vulkan Device function '{}'!", #_name); \
    }

    _VK_FUNCTION_NAMES_DEVICE
#undef _VK_DEFINE_FUNCTION

    return true;
}

}  // namespace lr::Graphics