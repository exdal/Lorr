// Created on Monday August 28th 2023 by exdal
// Last modified on Monday August 28th 2023 by exdal

#include "Vulkan.hh"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

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

    HMODULE libVulkan = LoadLibraryA("vulkan-1.dll");

    if (libVulkan == NULL)
    {
        LOG_CRITICAL("Failed to find Vulkan dynamic library.");
        FreeLibrary(libVulkan);
        return nullptr;
    }

#define _VK_DEFINE_FUNCTION(_name)                                 \
    _name = (PFN_##_name)GetProcAddress(libVulkan, #_name);        \
    if (_name == nullptr)                                          \
    {                                                              \
        LOG_CRITICAL("Cannot load Vulkan function '{}'!", #_name); \
        FreeLibrary(libVulkan);                                    \
        return nullptr;                                            \
    }

    _VK_FUNCTION_NAMES
#undef _VK_DEFINE_FUNCTION

    return libVulkan;
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
#if LR_DEBUG
    _VK_FUNCTION_NAMES_INSTANCE_DEBUG
#endif
    _VK_FUNCTION_NAMES_INSTANCE
#undef _VK_DEFINE_FUNCTION

    return true;
}

bool VK::LoadVulkanDevice(VkDevice pDevice)
{
    ZoneScoped;

#define _VK_DEFINE_FUNCTION(_name)                                        \
    _name = (PFN_##_name)vkGetDeviceProcAddr(pDevice, #_name);            \
    if (_name == nullptr)                                                 \
    {                                                                     \
        LOG_CRITICAL("Cannot load Vulkan Device function '{}'!", #_name); \
        return false;                                                     \
    }

    _VK_FUNCTION_NAMES_DEVICE
#undef _VK_DEFINE_FUNCTION

    return true;
}

}  // namespace lr::Graphics