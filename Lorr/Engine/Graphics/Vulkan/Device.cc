#include "Engine/Graphics/Vulkan/Impl.hh"

namespace lr {
constexpr Logger::Category to_log_category(VkDebugUtilsMessageSeverityFlagBitsEXT severity) {
    switch (severity) {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
            return Logger::INF;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            return Logger::WRN;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
            return Logger::ERR;
        default:
            break;
    }

    return Logger::DBG;
}

auto Device::create(usize frame_count) -> std::expected<Device, vk::Result> {
    ZoneScoped;

    auto impl = new Impl;
    impl->frame_count = frame_count;

    vkb::InstanceBuilder instance_builder;
    instance_builder.set_app_name("Lorr App");
    instance_builder.set_engine_name("Lorr");
    instance_builder.set_engine_version(1, 0, 0);
    instance_builder.enable_validation_layers(false);  // use vkconfig ui...
    instance_builder.request_validation_layers(false);
    instance_builder.set_debug_callback(
        [](VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
           VkDebugUtilsMessageTypeFlagsEXT messageType,
           const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
           [[maybe_unused]] void *pUserData) -> VkBool32 {
            auto type = vkb::to_string_message_type(messageType);
            auto category = to_log_category(messageSeverity);
            LOG(category,
                "[VK] "
                "{}:\n===========================================================\n{}\n===================================================="
                "=======",
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
        auto vk_error = instance_result.vk_result();

        LOG_ERROR("Failed to initialize Vulkan instance! {}", error.message());

        switch (vk_error) {
            case VK_ERROR_OUT_OF_HOST_MEMORY:
                return std::unexpected(vk::Result::OutOfHostMem);
            case VK_ERROR_OUT_OF_DEVICE_MEMORY:
                return std::unexpected(vk::Result::OutOfDeviceMem);
            case VK_ERROR_INITIALIZATION_FAILED:
                return std::unexpected(vk::Result::InitFailed);
            case VK_ERROR_LAYER_NOT_PRESENT:
                return std::unexpected(vk::Result::LayerNotPresent);
            case VK_ERROR_EXTENSION_NOT_PRESENT:
                return std::unexpected(vk::Result::ExtNotPresent);
            case VK_ERROR_INCOMPATIBLE_DRIVER:
                return std::unexpected(vk::Result::IncompatibleDriver);
            default:;
        }
    }

    impl->instance = instance_result.value();

    if (!load_vulkan_instance(impl->instance, impl->instance.fp_vkGetInstanceProcAddr)) {
        LOG_ERROR("Failed to create Vulkan Instance! Extension not found.");
        return std::unexpected(vk::Result::ExtNotPresent);
    }

    vkb::PhysicalDeviceSelector physical_device_selector(impl->instance);
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
    vk12_features.hostQueryReset = true;
    // Shader features
    vk12_features.scalarBlockLayout = true;
    vk12_features.shaderInt8 = true;
    physical_device_selector.set_required_features_12(vk12_features);

    VkPhysicalDeviceVulkan11Features vk11_features = {};
    vk11_features.variablePointers = true;
    vk11_features.variablePointersStorageBuffer = true;
    physical_device_selector.set_required_features_11(vk11_features);

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
        auto error = physical_device_result.error();

        LOG_ERROR("Failed to select Vulkan Physical Device! {}", error.message());
        return std::unexpected(vk::Result::DeviceLost);
    }

    impl->physical_device = physical_device_result.value();

    // We don't want to kill the coverage...
    // if (impl->physical_device.enable_extension_if_present("VK_EXT_descriptor_buffer")) {
    //     impl->supported_features |= DeviceFeature::DescriptorBuffer;
    //     VkPhysicalDeviceDescriptorBufferFeaturesEXT desciptor_buffer_features = {
    //         .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT,
    //         .pNext = nullptr,
    //         .descriptorBuffer = true,
    //         .descriptorBufferCaptureReplay = false,
    //         .descriptorBufferImageLayoutIgnored = true,
    //         .descriptorBufferPushDescriptors = true,
    //     };
    //     impl->physical_device.enable_extension_features_if_present(desciptor_buffer_features);
    // }
    if (impl->physical_device.enable_extension_if_present("VK_EXT_memory_budget")) {
        impl->supported_features |= DeviceFeature::MemoryBudget;
    }
    if (impl->physical_device.properties.limits.timestampPeriod != 0) {
        impl->supported_features |= DeviceFeature::QueryTimestamp;
    }

    vkb::DeviceBuilder device_builder(impl->physical_device);
    auto device_result = device_builder.build();
    if (!device_result) {
        auto error = device_result.error();
        auto vk_error = device_result.vk_result();

        LOG_ERROR("Failed to select Vulkan Device! {}", error.message());

        switch (vk_error) {
            case VK_ERROR_OUT_OF_HOST_MEMORY:
                return std::unexpected(vk::Result::OutOfHostMem);
            case VK_ERROR_OUT_OF_DEVICE_MEMORY:
                return std::unexpected(vk::Result::OutOfDeviceMem);
            case VK_ERROR_INITIALIZATION_FAILED:
                return std::unexpected(vk::Result::InitFailed);
            case VK_ERROR_EXTENSION_NOT_PRESENT:
                return std::unexpected(vk::Result::ExtNotPresent);
            case VK_ERROR_FEATURE_NOT_PRESENT:
                return std::unexpected(vk::Result::FeatureNotPresent);
            case VK_ERROR_TOO_MANY_OBJECTS:
                return std::unexpected(vk::Result::TooManyObjects);
            case VK_ERROR_DEVICE_LOST:
                return std::unexpected(vk::Result::DeviceLost);
            default:;
        }
    }

    impl->handle = device_result.value();

    if (!load_vulkan_device(impl->handle, impl->instance.fp_vkGetDeviceProcAddr)) {
        LOG_ERROR("Failed to create Vulkan Device! Extension not found.");
        return std::unexpected(vk::Result::ExtNotPresent);
    }

    set_object_name(impl, impl->instance, VK_OBJECT_TYPE_INSTANCE, "Instance");
    set_object_name(impl, impl->physical_device, VK_OBJECT_TYPE_PHYSICAL_DEVICE, "Physical Device");
    set_object_name(impl, impl->handle, VK_OBJECT_TYPE_DEVICE, "Device");

    auto create_command_queue = [&](vk::CommandType type, const std::string &debug_name) {};

    return Device(impl);
}

auto Device::destroy() -> void {
}

auto Device::queue(vk::CommandType type) -> CommandQueue {
    return impl->queues.at(std::to_underlying(type));
}

auto Device::wait() -> void {
}

auto Device::new_frame() -> usize {
}

auto Device::end_frame() -> void {
}

}  // namespace lr
