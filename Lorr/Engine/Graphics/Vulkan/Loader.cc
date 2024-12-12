#include "Engine/Graphics/Vulkan/Impl.hh"  // IWYU pragma: export

#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#define VMA_VULKAN_VERSION 1003000

#include <vk_mem_alloc.h>

#define LR_VULKAN_INSTANCE_FUNC(name, ...) PFN_##name name;
#define LR_VULKAN_DEVICE_FUNC(name, ...) PFN_##name name;
#include "Engine/Graphics/VulkanFunctions.hh"
#undef LR_VULKAN_DEVICE_FUNC
#undef LR_VULKAN_INSTANCE_FUNC

namespace lr {
bool load_vulkan_instance(VkInstance instance, PFN_vkGetInstanceProcAddr get_instance_proc_addr) {
#define LR_VULKAN_INSTANCE_FUNC(name, ...)                                \
    name = (PFN_##name)get_instance_proc_addr(instance, #name);           \
    if (name == nullptr) {                                                \
        LOG_ERROR("Failed to load Vulkan Instance function '{}'", #name); \
        return false;                                                     \
    }
#include "Engine/Graphics/VulkanFunctions.hh"
#undef LR_VULKAN_INSTANCE_FUNC

    return true;
}

bool load_vulkan_device(VkDevice device, PFN_vkGetDeviceProcAddr get_device_proc_addr) {
#define LR_VULKAN_DEVICE_FUNC(name, ...)                                \
    name = (PFN_##name)get_device_proc_addr(device, #name);             \
    if (name == nullptr) {                                              \
        LOG_ERROR("Failed to load Vulkan Device function '{}'", #name); \
    }
#include "Engine/Graphics/VulkanFunctions.hh"
#undef LR_VULKAN_DEVICE_FUNC

    return true;
}

}  // namespace lr
