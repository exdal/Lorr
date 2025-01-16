#include "Engine/Graphics/VulkanDevice.hh"

#include <vuk/runtime/ThisThreadExecutor.hpp>

namespace lr {
auto BindlessDescriptorSetLayoutInfo::add_binding(Descriptors binding, VkDescriptorType type, u32 count) -> void {
    ZoneScoped;

    pool_size += count;
    layout_info.bindings.push_back({
        .binding = std::to_underlying(binding),
        .descriptorType = type,
        .descriptorCount = count,
        .stageFlags = VK_SHADER_STAGE_ALL,
        .pImmutableSamplers = nullptr,
    });
    layout_info.flags.push_back(VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT);
}

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

auto Device::init(this Device &self, usize frame_count) -> std::expected<void, vuk::VkException> {
    ZoneScoped;

    self.frames_in_flight = frame_count;

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

    std::vector<const c8 *> instance_extensions;
    instance_extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
    instance_extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
#if defined(LS_WINDOWS)
    instance_extensions.push_back("VK_KHR_win32_surface");
#elif defined(LS_LINUX)
    instance_extensions.push_back("VK_KHR_xcb_surface");
    instance_extensions.push_back("VK_KHR_wayland_surface");
#endif
#if LS_DEBUG
    instance_extensions.push_back("VK_EXT_debug_utils");
#endif

    instance_builder.enable_extensions(instance_extensions);
    instance_builder.require_api_version(1, 3, 0);
    auto instance_result = instance_builder.build();
    if (!instance_result) {
        auto error = instance_result.error();
        auto vk_error = instance_result.vk_result();

        LOG_ERROR("Failed to initialize Vulkan instance! {}", error.message());

        return std::unexpected(vk_error);
    }

    self.instance = instance_result.value();

    vkb::PhysicalDeviceSelector physical_device_selector(self.instance);
    physical_device_selector.defer_surface_initialization();
    physical_device_selector.set_minimum_version(1, 3);

    VkPhysicalDeviceVulkan13Features vk13_features = {};
    vk13_features.synchronization2 = true;
    vk13_features.shaderDemoteToHelperInvocation = true;
    physical_device_selector.set_required_features_13(vk13_features);

    VkPhysicalDeviceVulkan12Features vk12_features = {};
    vk12_features.descriptorIndexing = true;
    vk12_features.shaderOutputLayer = true;
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

    std::vector<const c8 *> device_extensions;
    device_extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    device_extensions.push_back(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME);
    physical_device_selector.add_required_extensions(device_extensions);

    auto physical_device_result = physical_device_selector.select();
    if (!physical_device_result) {
        auto error = physical_device_result.error();

        LOG_ERROR("Failed to select Vulkan Physical Device! {}", error.message());
        return std::unexpected(VK_ERROR_DEVICE_LOST);
    }

    self.physical_device = physical_device_result.value();

    if (self.physical_device.enable_extension_if_present("VK_EXT_memory_budget")) {
        self.supported_features |= DeviceFeature::MemoryBudget;

        LOG_INFO("Memory Budget extension enabled.");
    }
    if (self.physical_device.properties.limits.timestampPeriod != 0) {
        self.supported_features |= DeviceFeature::QueryTimestamp;

        LOG_INFO("Query Timestamp extension enabled.");
    }

    vkb::DeviceBuilder device_builder(self.physical_device);
    auto device_result = device_builder.build();
    if (!device_result) {
        auto error = device_result.error();
        auto vk_error = device_result.vk_result();

        LOG_ERROR("Failed to select Vulkan Device! {}", error.message());
        return std::unexpected(vk_error);
    }

    self.handle = device_result.value();

    vuk::FunctionPointers vulkan_functions;
    vulkan_functions.vkGetInstanceProcAddr = self.instance.fp_vkGetInstanceProcAddr;
    vulkan_functions.vkGetDeviceProcAddr = self.handle.fp_vkGetDeviceProcAddr;
    vulkan_functions.load_pfns(self.instance, self.handle, true);

    std::vector<std::unique_ptr<vuk::Executor>> executors;

    auto graphics_queue = self.handle.get_queue(vkb::QueueType::graphics).value();
    auto graphics_queue_family_index = self.handle.get_queue_index(vkb::QueueType::graphics).value();
    executors.push_back(vuk::create_vkqueue_executor(
        vulkan_functions, self.handle, graphics_queue, graphics_queue_family_index, vuk::DomainFlagBits::eGraphicsQueue));

    auto compute_queue = self.handle.get_queue(vkb::QueueType::compute).value();
    auto compute_queue_family_index = self.handle.get_queue_index(vkb::QueueType::compute).value();
    executors.push_back(vuk::create_vkqueue_executor(
        vulkan_functions, self.handle, compute_queue, compute_queue_family_index, vuk::DomainFlagBits::eComputeQueue));

    auto transfer_queue = self.handle.get_queue(vkb::QueueType::transfer).value();
    auto transfer_queue_family_index = self.handle.get_queue_index(vkb::QueueType::transfer).value();
    executors.push_back(vuk::create_vkqueue_executor(
        vulkan_functions, self.handle, transfer_queue, transfer_queue_family_index, vuk::DomainFlagBits::eTransferQueue));

    executors.push_back(std::make_unique<vuk::ThisThreadExecutor>());
    self.runtime.emplace(vuk::RuntimeCreateParameters{
        .instance = self.instance,
        .device = self.handle,
        .physical_device = self.physical_device,
        .executors = std::move(executors),
        .pointers = vulkan_functions,
    });

    self.frame_resources.emplace(*self.runtime, frame_count);
    self.allocator.emplace(*self.frame_resources);
    self.runtime->set_shader_target_version(VK_API_VERSION_1_3);

    auto bindless_layout = self.get_bindless_descriptor_set_layout();
    self.descriptor_set =
        self.runtime->create_persistent_descriptorset(*self.allocator, bindless_layout.layout_info, bindless_layout.pool_size);

    // I would rather a crashed renderer than a renderer without BDA buffer
    self.bda_array_buffer = Buffer::create(self, self.resources.buffers.max_resources() * sizeof(u64), vuk::MemoryUsage::eCPUtoGPU).value();

    self.descriptor_set->update_storage_buffer(std::to_underlying(Descriptors::BDA), 0, *self.buffer(self.bda_array_buffer.id()));
    self.descriptor_set->commit(*self.runtime);

    self.transfer_manager.init(self).value();
    self.shader_compiler = SlangCompiler::create().value();

    return {};
}

auto Device::destroy(this Device &self) -> void {
    ZoneScoped;

    self.wait();
    self.resources.buffers.reset();
    self.resources.images.reset();
    self.resources.image_views.reset();
    self.resources.samplers.reset();
    self.resources.pipelines.reset();

    self.transfer_manager.destroy();
    self.shader_compiler.destroy();

    self.descriptor_set.reset();
    self.frame_resources.reset();
    self.allocator.reset();
    self.runtime.reset();
    self.compiler.reset();
}

auto Device::new_slang_session(this Device &self, const SlangSessionInfo &info) -> ls::option<SlangSession> {
    return self.shader_compiler.new_session(info);
}

auto Device::transfer_man(this Device &self) -> TransferManager & {
    ZoneScoped;

    return self.transfer_manager;
}

auto Device::new_frame(this Device &self, SwapChain &swapchain) -> vuk::Value<vuk::ImageAttachment> {
    ZoneScoped;

    self.gpu_profiler_tasks.clear();
    self.runtime->next_frame();

    auto acquired_swapchain = vuk::acquire_swapchain(swapchain.handle_.value());
    auto acquired_image = vuk::acquire_next_image("present_image", std::move(acquired_swapchain));

    auto &frame_resource = self.frame_resources->get_next_frame();
    self.transfer_manager.acquire(frame_resource);

    return acquired_image;
}

auto Device::end_frame(this Device &self, vuk::Value<vuk::ImageAttachment> &&target_attachment) -> void {
    ZoneScoped;

    self.transfer_manager.wait_for_ops(*self.allocator, self.compiler);

    auto result = vuk::enqueue_presentation(std::move(target_attachment));
    result.submit(*self.transfer_manager.frame_allocator, self.compiler, { .graph_label = {}, .callbacks = {} });
    self.transfer_manager.release();
}

auto Device::wait() -> void {
    ZoneScoped;

    runtime->wait_idle();
}

auto Device::set_name(this Device &self, Buffer &buffer, std::string_view name) -> void {
    ZoneScoped;

    self.runtime->set_name(self.buffer(buffer.id())->buffer, name);
}

auto Device::set_name(this Device &self, Image &image, std::string_view name) -> void {
    ZoneScoped;

    self.runtime->set_name(self.image(image.id())->image, name);
}

auto Device::set_name(this Device &self, ImageView &image_view, std::string_view name) -> void {
    ZoneScoped;

    self.runtime->set_name(self.image_view(image_view.id())->payload, name);
}

auto Device::frame_count(this const Device &self) -> usize {
    return self.frames_in_flight;
}

auto Device::buffer(this Device &self, BufferID id) -> vuk::Buffer * {
    ZoneScoped;

    return &*self.resources.buffers.get(id);
}

auto Device::image(this Device &self, ImageID id) -> vuk::Image * {
    ZoneScoped;

    return &*self.resources.images.get(id);
}

auto Device::image_view(this Device &self, ImageViewID id) -> vuk::ImageView * {
    ZoneScoped;

    return &*self.resources.image_views.get(id);
}

auto Device::sampler(this Device &self, SamplerID id) -> vuk::Sampler * {
    ZoneScoped;

    return &self.resources.samplers.get(id);
}

auto Device::pipeline(this Device &self, PipelineID id) -> vuk::PipelineBaseInfo ** {
    ZoneScoped;

    return &self.resources.pipelines.get(id);
}

auto Device::bindless_descriptor_set() -> vuk::PersistentDescriptorSet & {
    return descriptor_set.get();
}

auto Device::feature_supported(this Device &self, DeviceFeature feature) -> bool {
    ZoneScoped;
    return self.supported_features & feature;
}

auto Device::get_bindless_descriptor_set_layout() -> BindlessDescriptorSetLayoutInfo {
    BindlessDescriptorSetLayoutInfo bindless_layout;
    bindless_layout.add_binding(Descriptors::Samplers, VK_DESCRIPTOR_TYPE_SAMPLER, 1000);
    bindless_layout.add_binding(Descriptors::Images, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000);
    bindless_layout.add_binding(Descriptors::StorageImages, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000);
    bindless_layout.add_binding(Descriptors::StorageBuffers, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000);
    bindless_layout.add_binding(Descriptors::BDA, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1);

    return bindless_layout;
}

}  // namespace lr
