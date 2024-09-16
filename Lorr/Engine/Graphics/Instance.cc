#include "Instance.hh"

namespace lr {
constexpr loguru::Verbosity to_loguru_severity(VkDebugUtilsMessageSeverityFlagBitsEXT severity) {
    switch (severity) {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
            return loguru::Verbosity_INFO;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            return loguru::Verbosity_WARNING;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
            return loguru::Verbosity_ERROR;
        default:
            break;
    }

    return loguru::Verbosity_0;
}

VKResult Instance::init(const InstanceInfo &info) {
    ZoneScoped;

    vkb::InstanceBuilder instance_builder;
    instance_builder.set_app_name(info.app_name.data());
    instance_builder.set_engine_name(info.engine_name.data());
    instance_builder.set_engine_version(1, 0, 0);
    instance_builder.enable_validation_layers(false);  // use vkconfig ui...
    instance_builder.request_validation_layers(false);
    instance_builder.set_debug_callback(
        [](VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
           VkDebugUtilsMessageTypeFlagsEXT messageType,
           const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
           [[maybe_unused]] void *pUserData) -> VkBool32 {
            auto type = vkb::to_string_message_type(messageType);
            VLOG_F(
                to_loguru_severity(messageSeverity),
                "[VK] {}:\n===========================================================\n{}\n===========================================================",
                type,
                pCallbackData->pMessage);
            return VK_FALSE;
        });

    instance_builder.enable_extensions({
        "VK_KHR_surface",
        "VK_KHR_get_physical_device_properties2",
#if defined(LS_WINDOWS)
        "VK_KHR_win32_surface",
#elif defined(LS_LINUX)
        "VK_KHR_xcb_surface",
#endif
#if LS_DEBUG
        //! Debug extensions, always put it to bottom
        "VK_EXT_debug_utils",
        "VK_EXT_debug_report",
#endif
    });
    instance_builder.require_api_version(1, 3, 0);
    auto instance_result = instance_builder.build();
    if (!instance_result) {
        auto error = instance_result.error();
        auto r = static_cast<VKResult>(instance_result.vk_result());
        LR_LOG_ERROR("Failed to create Vulkan Instance! {} - {}", error.message(), r);
        return r;
    }

    handle = instance_result.value();

    if (!vulkan::load_instance(handle, handle.fp_vkGetInstanceProcAddr)) {
        LR_LOG_ERROR("Failed to create Vulkan Instance! Extension not found.");
        return VKResult::ExtNotPresent;
    }

    return VKResult::Success;
}

}  // namespace lr
