#include "Vulkan.hh"

#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#define VMA_VULKAN_VERSION 1003000

#include <vk_mem_alloc.h>

#define VKFN_FUNCTION(_name) PFN_##_name _name
VKFN_INSTANCE_FUNCTIONS
VKFN_PHYSICAL_DEVICE_FUNCTIONS
VKFN_LOGICAL_DEVICE_FUNCTIONS
VKFN_COMMAND_BUFFER_FUNCTIONS
VKFN_DESCRIPTOR_BUFFER_EXT_FUNCTIONS
VKFN_CALIBRATED_TIMESTAMPS_EXT_DEVICE_FUNCTIONS
VKFN_CALIBRATED_TIMESTAMPS_EXT_INSTANCE_FUNCTIONS
VKFN_DEBUG_UTILS_EXT_DEVICE_FUNCTIONS
#undef VKFN_FUNCTION

namespace lr::graphics {
bool vulkan::load_instance(VkInstance instance, PFN_vkGetInstanceProcAddr get_instance_proc_addr)
{
#define VKFN_FUNCTION(_name)                                                   \
    _name = (PFN_##_name)get_instance_proc_addr(instance, #_name);             \
    if ((_name) == nullptr) {                                                  \
        LR_LOG_TRACE("Failed to load Vulkan Instance function '{}'.", #_name); \
        return false;                                                          \
    }

    VKFN_INSTANCE_FUNCTIONS
    VKFN_PHYSICAL_DEVICE_FUNCTIONS
#if TRACY_ENABLE
    VKFN_CALIBRATED_TIMESTAMPS_EXT_INSTANCE_FUNCTIONS
#endif
#undef VKFN_FUNCTION

    return true;
}

bool vulkan::load_device(VkDevice device, PFN_vkGetDeviceProcAddr get_device_proc_addr)
{
#define VKFN_FUNCTION(_name)                                                 \
    _name = (PFN_##_name)get_device_proc_addr(device, #_name);               \
    if ((_name) == nullptr) {                                                \
        LR_LOG_ERROR("Failed to load Vulkan Device function '{}'.", #_name); \
        return false;                                                        \
    }

    VKFN_LOGICAL_DEVICE_FUNCTIONS
    VKFN_COMMAND_BUFFER_FUNCTIONS
#if LR_DEBUG
    VKFN_DEBUG_UTILS_EXT_DEVICE_FUNCTIONS
#endif
#if TRACY_ENABLE
    VKFN_CALIBRATED_TIMESTAMPS_EXT_DEVICE_FUNCTIONS
#endif
#undef VKFN_FUNCTION

    // OPTIONAL FUNCTIONS, MAINLY THERE IS AN ALTERNATIVE VERSION(EXTENSION) FOR THEM.
    // EXTENSIONS MUST BE CHECKED BEFORE LOADING
#define VKFN_FUNCTION(_name) _name = (PFN_##_name)get_device_proc_addr(device, #_name);
    VKFN_DESCRIPTOR_BUFFER_EXT_FUNCTIONS
#undef VKFN_FUNCTION

    return true;
}
}  // namespace lr::graphics
