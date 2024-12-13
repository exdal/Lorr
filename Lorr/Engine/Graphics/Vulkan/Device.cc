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
                "{}:\n========================================================="
                "=="
                "\n{}\n===================================================="
                "=======",
                type,
                pCallbackData->pMessage);
            return VK_FALSE;
        });

    // For some reason, indentation gets fucked so disable clang-format
    // clang-format off
    instance_builder.enable_extensions({
        "VK_KHR_surface",
        "VK_KHR_get_physical_device_properties2",
#if defined(LS_WINDOWS)
        "VK_KHR_win32_surface",
#elif defined(LS_LINUX)
        "VK_KHR_xlib_surface",
#endif
#if LS_DEBUG
        //! Debug extensions, always put it to bottom
        "VK_EXT_debug_utils",
#endif
    });
    // clang-format on

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
    vk13_features.shaderDemoteToHelperInvocation = true;
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

        LOG_INFO("Memory Budget extension enabled.");
    }
    if (impl->physical_device.properties.limits.timestampPeriod != 0) {
        impl->supported_features |= DeviceFeature::QueryTimestamp;

        LOG_INFO("Query Timestamp extension enabled.");
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

    set_object_name(impl, impl->instance.instance, VK_OBJECT_TYPE_INSTANCE, "Instance");
    set_object_name(impl, impl->physical_device.physical_device, VK_OBJECT_TYPE_PHYSICAL_DEVICE, "Physical Device");
    set_object_name(impl, impl->handle.device, VK_OBJECT_TYPE_DEVICE, "Device");

    impl->queues[0] = CommandQueue::create(impl, vk::CommandType::Graphics).value().set_name("Graphics Queue");
    impl->queues[1] = CommandQueue::create(impl, vk::CommandType::Compute).value().set_name("Compute Queue");
    impl->queues[2] = CommandQueue::create(impl, vk::CommandType::Transfer).value().set_name("Transfer Queue");
    impl->frame_sema = Semaphore::create(impl, 0).value().set_name("Device Frame Semaphore");

    VmaVulkanFunctions vulkan_functions = {};
    vulkan_functions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
    vulkan_functions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

    VmaAllocatorCreateInfo allocator_create_info = {
        .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
        .physicalDevice = impl->physical_device,
        .device = impl->handle,
        .preferredLargeHeapBlockSize = 0,
        .pAllocationCallbacks = nullptr,
        .pDeviceMemoryCallbacks = nullptr,
        .pHeapSizeLimit = nullptr,
        .pVulkanFunctions = &vulkan_functions,
        .instance = impl->instance,
        .vulkanApiVersion = VK_API_VERSION_1_3,
        .pTypeExternalMemoryHandleTypes = nullptr,
    };
    auto allocator_result = vmaCreateAllocator(&allocator_create_info, &impl->allocator);
    if (allocator_result != VK_SUCCESS) {
        LOG_ERROR("Failed to create VMA");
        return std::unexpected(vk::Result::Unknown);
    }

    VkDescriptorBindingFlags bindless_flags = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
    std::vector<VkDescriptorPoolSize> pool_sizes;
    std::vector<VkDescriptorSetLayoutBinding> layout_elements;
    std::vector<VkDescriptorBindingFlags> binding_flags;
    auto add_descriptor_binding = [&](Descriptors binding, VkDescriptorType type, u32 count) {
        pool_sizes.push_back({
            .type = type,
            .descriptorCount = count,
        });
        layout_elements.push_back({
            .binding = std::to_underlying(binding),
            .descriptorType = type,
            .descriptorCount = count,
            .stageFlags = VK_SHADER_STAGE_ALL,
            .pImmutableSamplers = nullptr,
        });
        binding_flags.push_back(bindless_flags);
    };

    add_descriptor_binding(Descriptors::Samplers, VK_DESCRIPTOR_TYPE_SAMPLER, impl->resources.samplers.max_resources());
    add_descriptor_binding(Descriptors::Images, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, impl->resources.images.max_resources());
    add_descriptor_binding(Descriptors::StorageImages, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, impl->resources.image_views.max_resources());
    add_descriptor_binding(Descriptors::StorageBuffers, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, impl->resources.buffers.max_resources());
    add_descriptor_binding(Descriptors::BDA, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1);

    VkDescriptorPoolCreateInfo descriptor_pool_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
        .maxSets = 1,
        .poolSizeCount = static_cast<u32>(pool_sizes.size()),
        .pPoolSizes = pool_sizes.data(),
    };
    auto descriptor_pool_result = vkCreateDescriptorPool(impl->handle, &descriptor_pool_info, nullptr, &impl->descriptor_pool);
    switch (descriptor_pool_result) {
        case VK_ERROR_OUT_OF_HOST_MEMORY:
            LOG_ERROR("Failed to create Descriptor Pool, out of host mem");
            return std::unexpected(vk::Result::OutOfHostMem);
        case VK_ERROR_OUT_OF_DEVICE_MEMORY:
            LOG_ERROR("Failed to create Descriptor Pool, out of device mem");
            return std::unexpected(vk::Result::OutOfDeviceMem);
        default:;
    }

    VkDescriptorSetLayoutBindingFlagsCreateInfo binding_flags_create_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
        .pNext = nullptr,
        .bindingCount = static_cast<u32>(binding_flags.size()),
        .pBindingFlags = binding_flags.data(),
    };

    VkDescriptorSetLayoutCreateInfo descriptor_set_layout_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = &binding_flags_create_info,
        .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
        .bindingCount = static_cast<u32>(layout_elements.size()),
        .pBindings = layout_elements.data(),
    };
    auto descriptor_set_layout =
        vkCreateDescriptorSetLayout(impl->handle, &descriptor_set_layout_info, nullptr, &impl->descriptor_set_layout);
    switch (descriptor_set_layout) {
        case VK_ERROR_OUT_OF_HOST_MEMORY:
            LOG_ERROR("Failed to create Descriptor Set Layout, out of host mem");
            return std::unexpected(vk::Result::OutOfHostMem);
        case VK_ERROR_OUT_OF_DEVICE_MEMORY:
            LOG_ERROR("Failed to create Descriptor Set Layout, out of device mem");
            return std::unexpected(vk::Result::OutOfDeviceMem);
        default:;
    }

    VkDescriptorSetAllocateInfo descriptor_set_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = nullptr,
        .descriptorPool = impl->descriptor_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &impl->descriptor_set_layout,
    };
    auto descriptor_set_result = vkAllocateDescriptorSets(impl->handle, &descriptor_set_info, &impl->descriptor_set);
    switch (descriptor_set_result) {
        case VK_ERROR_OUT_OF_HOST_MEMORY:
            LOG_ERROR("Failed to create Descriptor Set, out of host mem");
            return std::unexpected(vk::Result::OutOfHostMem);
        case VK_ERROR_OUT_OF_DEVICE_MEMORY:
            LOG_ERROR("Failed to create Descriptor Set, out of device mem");
            return std::unexpected(vk::Result::OutOfDeviceMem);
        case VK_ERROR_FRAGMENTED_POOL:
            LOG_ERROR("Failed to create Descriptor Set, fragmented pool");
            return std::unexpected(vk::Result::FragmentedPool);
        default:;
    }

    set_object_name(impl, impl->descriptor_pool, VK_OBJECT_TYPE_DESCRIPTOR_POOL, "Bindless Descriptor Pool");
    set_object_name(impl, impl->descriptor_set_layout, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, "Bindless Descriptor Set Layout");
    set_object_name(impl, impl->descriptor_set, VK_OBJECT_TYPE_DESCRIPTOR_SET, "Bindless Descriptor Set");

    for (u32 i = 0; i < impl->resources.pipeline_layouts.size(); i++) {
        auto &layout = impl->resources.pipeline_layouts.at(i);
        VkPushConstantRange push_constant_range = {
            .stageFlags = VK_SHADER_STAGE_ALL,
            .offset = 0,
            .size = static_cast<u32>(i * sizeof(u32)),
        };
        VkPipelineLayoutCreateInfo pipeline_layout_create_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .setLayoutCount = 1,
            .pSetLayouts = &impl->descriptor_set_layout,
            .pushConstantRangeCount = !!i,
            .pPushConstantRanges = &push_constant_range,
        };

        vkCreatePipelineLayout(impl->handle, &pipeline_layout_create_info, nullptr, &layout);
        set_object_name(impl, layout, VK_OBJECT_TYPE_PIPELINE_LAYOUT, std::format("Pipeline Layout ({})", i));
    }

    // I would rather a crashed renderer than a renderer without BDA buffer
    impl->bda_array_buffer = Buffer::create(
                                 impl,
                                 vk::BufferUsage::Storage,
                                 impl->resources.buffers.max_resources() * sizeof(u64),
                                 vk::MemoryAllocationUsage::PreferDevice,
                                 vk::MemoryAllocationFlag::HostSeqWrite)
                                 .value()
                                 .set_name("BDA Array Buffer");

    VkDescriptorBufferInfo bda_descriptor_info = {
        .buffer = impl->bda_array_buffer->handle,
        .offset = 0,
        .range = ~0_u64,
    };

    VkWriteDescriptorSet bda_write_descriptor_set_info = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,
        .dstSet = impl->descriptor_set,
        .dstBinding = std::to_underlying(Descriptors::BDA),
        .dstArrayElement = std::to_underlying(impl->bda_array_buffer.id()),
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .pImageInfo = nullptr,
        .pBufferInfo = &bda_descriptor_info,
        .pTexelBufferView = nullptr,
    };

    vkUpdateDescriptorSets(impl->handle, 1, &bda_write_descriptor_set_info, 0, nullptr);

    impl->transfer_manager = TransferManager::create(impl, Limits::PersistentBufferSize);
    impl->shader_compiler = SlangCompiler::create().value();

    return Device(impl);
}

auto Device::destroy() -> void {
}

auto Device::frame_count() const -> usize {
    return impl->frame_count;
}

auto Device::frame_sema() const -> Semaphore {
    return impl->frame_sema;
}

auto Device::queue(vk::CommandType type) const -> CommandQueue {
    return impl->queues.at(std::to_underlying(type));
}

auto Device::new_slang_session(const SlangSessionInfo &info) -> ls::option<SlangSession> {
    return impl->shader_compiler.new_session(info);
}

auto Device::wait() const -> void {
    ZoneScoped;

    vkDeviceWaitIdle(impl->handle);
}

auto Device::transfer_man() const -> TransferManager & {
    ZoneScoped;

    return impl->transfer_manager;
}

auto Device::new_frame() -> usize {
    ZoneScoped;

    i64 sema_counter = static_cast<i64>(impl->frame_sema.value());
    u64 wait_val = static_cast<u64>(std::max<i64>(0, sema_counter - static_cast<i64>(impl->frame_count - 1)));

    impl->frame_sema.wait(wait_val);
    return impl->frame_sema.value() % impl->frame_count;
}

auto Device::end_frame(SwapChain &swap_chain, usize semaphore_index) -> void {
    ZoneScoped;

    auto present_queue = this->queue(vk::CommandType::Graphics);
    auto present_queue_sema = present_queue.semaphore();

    auto [_, present_sema] = swap_chain.semaphores(semaphore_index);
    Semaphore signal_semas[] = { present_sema, impl->frame_sema, present_queue_sema };
    present_queue.submit(present_queue_sema, signal_semas);
    present_queue.present(swap_chain, present_sema, swap_chain.image_index());

    impl->transfer_manager.collect_garbage();
    for (auto &v : impl->queues) {
        v.collect_garbage();
    }
}

auto Device::buffer(BufferID id) const -> Buffer {
    ZoneScoped;
    return Buffer(&impl->resources.buffers.get(id));
}

auto Device::image(ImageID id) const -> Image {
    ZoneScoped;
    return Image(&impl->resources.images.get(id));
}

auto Device::image_view(ImageViewID id) const -> ImageView {
    ZoneScoped;
    return ImageView(&impl->resources.image_views.get(id));
}

auto Device::sampler(SamplerID id) const -> Sampler {
    ZoneScoped;
    return Sampler(&impl->resources.samplers.get(id));
}

auto Device::pipeline(PipelineID id) const -> Pipeline {
    ZoneScoped;
    return Pipeline(&impl->resources.pipelines.get(id));
}

auto Device::feature_supported(DeviceFeature feature) -> bool {
    ZoneScoped;
    return impl->supported_features & feature;
}

}  // namespace lr
