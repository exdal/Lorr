#include "Instance.hh"

#include "Device.hh"

namespace lr::graphics {
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
            auto severity = vkb::to_string_message_severity(messageSeverity);
            auto type = vkb::to_string_message_type(messageType);
            LR_LOG_TRACE("[{}: {}] {}\n", severity, type, pCallbackData->pMessage);
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
    physical_device_selector.set_required_features_13({
        .synchronization2 = true,
        .dynamicRendering = true,
    });
    physical_device_selector.set_required_features_12({
        .descriptorIndexing = true,
        .shaderSampledImageArrayNonUniformIndexing = true,
        .shaderStorageBufferArrayNonUniformIndexing = true,
        .descriptorBindingSampledImageUpdateAfterBind = true,
        .descriptorBindingStorageImageUpdateAfterBind = true,
        .descriptorBindingStorageBufferUpdateAfterBind = true,
        .descriptorBindingUpdateUnusedWhilePending = true,
        .descriptorBindingPartiallyBound = true,
        .descriptorBindingVariableDescriptorCount = true,
        .runtimeDescriptorArray = true,
        .timelineSemaphore = true,
        .bufferDeviceAddress = true,
    });
    physical_device_selector.set_required_features({
        .vertexPipelineStoresAndAtomics = true,
        .fragmentStoresAndAtomics = true,
        .shaderInt64 = true,
    });
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
