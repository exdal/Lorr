#include "Instance.hh"

#include "Device.hh"

namespace lr::graphics {
constexpr loguru::Verbosity to_loguru_severity(VkDebugUtilsMessageSeverityFlagBitsEXT severity)
{
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

VKResult Instance::init(const InstanceInfo &info)
{
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
            VLOG_F(to_loguru_severity(messageSeverity), "[VK] {}: {}\n", type, pCallbackData->pMessage);
            return VK_FALSE;
        });

    instance_builder.enable_extensions({
        "VK_KHR_surface",
        "VK_KHR_get_physical_device_properties2",
#if defined(LR_WIN32)
        "VK_KHR_win32_surface",
#elif defined(LR_LINUX)
        "VK_KHR_xcb_surface",
#endif
#if _DEBUG
        //! Debug extensions, always put it to bottom
        "VK_EXT_debug_utils",
        "VK_EXT_debug_report",
#endif
    });
    instance_builder.require_api_version(1, 3, 0);
    auto instance_result = instance_builder.build();
    if (!instance_result) {
        auto error = static_cast<VKResult>(instance_result.vk_result());
        LR_LOG_ERROR("Failed to create Vulkan Instance! {}", error);
        return error;
    }

    m_handle = instance_result.value();

    if (!vulkan::load_instance(m_handle, m_handle.fp_vkGetInstanceProcAddr)) {
        LR_LOG_ERROR("Failed to create Vulkan Instance! Extension not found.");
        return VKResult::ExtNotPresent;
    }

    return VKResult::Success;
}

Result<Device, VKResult> Instance::create_device()
{
    vkb::PhysicalDeviceSelector physical_device_selector(m_handle);
    physical_device_selector.defer_surface_initialization();
    physical_device_selector.set_minimum_version(1, 3);

    VkPhysicalDeviceVulkan13Features vk13_features = {};
    vk13_features.synchronization2 = true;
    vk13_features.dynamicRendering = true;
    physical_device_selector.set_required_features_13(vk13_features);

    VkPhysicalDeviceVulkan12Features vk12_features = {};
    vk12_features.descriptorIndexing = true;
    vk12_features.shaderSampledImageArrayNonUniformIndexing = true;
    vk12_features.shaderStorageBufferArrayNonUniformIndexing = true;
    vk12_features.descriptorBindingSampledImageUpdateAfterBind = true;
    vk12_features.descriptorBindingStorageImageUpdateAfterBind = true;
    vk12_features.descriptorBindingStorageBufferUpdateAfterBind = true;
    vk12_features.descriptorBindingUpdateUnusedWhilePending = true;
    vk12_features.descriptorBindingPartiallyBound = true;
    vk12_features.descriptorBindingVariableDescriptorCount = true;
    vk12_features.runtimeDescriptorArray = true;
    vk12_features.timelineSemaphore = true;
    vk12_features.bufferDeviceAddress = true;
    physical_device_selector.set_required_features_12(vk12_features);

    VkPhysicalDeviceFeatures vk10_features = {};
    vk10_features.vertexPipelineStoresAndAtomics = true;
    vk10_features.fragmentStoresAndAtomics = true;
    vk10_features.shaderInt64 = true;
    physical_device_selector.set_required_features(vk10_features);
    physical_device_selector.add_required_extensions({
        "VK_KHR_swapchain",
#if TRACY_ENABLE
        "VK_EXT_calibrated_timestamps",
#endif
    });
    auto physical_device_result = physical_device_selector.select();
    if (!physical_device_result) {
        LR_LOG_ERROR("Failed to select Vulkan Physical Device!");
        return static_cast<VKResult>(physical_device_result.vk_result());
    }

    vkb::PhysicalDevice &physical_device = physical_device_result.value();

    /// DEVICE INITIALIZATION ///
    DeviceFeature features = {};

    // if
    // (selected_physical_device.enable_extension_if_present("VK_EXT_descriptor_buffer"))
    //     features |= DeviceFeature::DescriptorBuffer;
    if (physical_device.enable_extension_if_present("VK_EXT_memory_budget"))
        features |= DeviceFeature::MemoryBudget;

    vkb::DeviceBuilder device_builder(physical_device);

    VkPhysicalDeviceDescriptorBufferFeaturesEXT desciptor_buffer_features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT,
        .pNext = nullptr,
        .descriptorBuffer = true,
        .descriptorBufferCaptureReplay = false,
        .descriptorBufferImageLayoutIgnored = true,
        .descriptorBufferPushDescriptors = true,
    };
    if (features & DeviceFeature::DescriptorBuffer)
        device_builder.add_pNext(&desciptor_buffer_features);

    auto device_result = device_builder.build();
    if (!device_result) {
        LR_LOG_ERROR("Failed to select Vulkan Device!");
        return static_cast<VKResult>(device_result.vk_result());
    }

    vkb::Device &device = device_result.value();

    if (!vulkan::load_device(device, m_handle.fp_vkGetDeviceProcAddr)) {
        LR_LOG_ERROR("Failed to create Vulkan Device! Extension not found.");
        return VKResult::ExtNotPresent;
    }

    return Device(std::move(device), &m_handle, features);
}

}  // namespace lr::graphics
