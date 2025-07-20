#include "Engine/Graphics/VulkanDevice.hh"

#include <vuk/runtime/ThisThreadExecutor.hpp>

namespace lr {
constexpr fmtlog::LogLevel to_log_category(VkDebugUtilsMessageSeverityFlagBitsEXT severity) {
    switch (severity) {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
            return fmtlog::INF;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            return fmtlog::WRN;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
            return fmtlog::ERR;
        default:
            break;
    }

    return fmtlog::DBG;
}

auto Device::init(this Device &self, usize frame_count) -> std::expected<void, vuk::VkException> {
    ZoneScoped;

    self.frames_in_flight = frame_count;

    vkb::InstanceBuilder instance_builder;
    instance_builder.set_app_name("Lorr App");
    instance_builder.set_engine_name("Lorr");
    instance_builder.set_engine_version(1, 0, 0);
    instance_builder.enable_validation_layers(false); // use vkconfig ui...
    instance_builder.request_validation_layers(false);
    instance_builder.set_debug_callback(
        [](VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
           VkDebugUtilsMessageTypeFlagsEXT messageType,
           const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
           [[maybe_unused]] void *pUserData) -> VkBool32 {
            auto type = vkb::to_string_message_type(messageType);
            auto category = to_log_category(messageSeverity);
            FMTLOG(category, "[VK] {}: {}", type, pCallbackData->pMessage);
            return VK_FALSE;
        }
    );

    std::vector<const c8 *> instance_extensions;
    instance_extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
    instance_extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
#if defined(LS_WINDOWS)
    instance_extensions.push_back("VK_KHR_win32_surface");
#elif defined(LS_LINUX)
    instance_extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
    instance_extensions.push_back("VK_KHR_xcb_surface");
    instance_extensions.push_back("VK_KHR_xlib_surface");
    // instance_extensions.push_back("VK_KHR_wayland_surface");
#endif
#if LS_DEBUG
    instance_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

    instance_builder.enable_extensions(instance_extensions);
    instance_builder.require_api_version(1, 3, 0);
    auto instance_result = instance_builder.build();
    if (!instance_result) {
        auto error = instance_result.error();
        auto vk_error = instance_result.vk_result();

        LOG_ERROR("Failed to initialize Vulkan instance! {}-{}", error.message(), std::to_underlying(vk_error));

        return std::unexpected(vk_error);
    }

    self.instance = instance_result.value();

    LOG_TRACE("Created device.");

    vkb::PhysicalDeviceSelector physical_device_selector(self.instance);
    physical_device_selector.defer_surface_initialization();
    physical_device_selector.disable_portability_subset();
    physical_device_selector.set_minimum_version(1, 3);

    std::vector<const c8 *> device_extensions;
    device_extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    device_extensions.push_back(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME);
    device_extensions.push_back(VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME);
    //device_extensions.push_back(VK_EXT_SHADER_IMAGE_ATOMIC_INT64_EXTENSION_NAME);
    //device_extensions.push_back(VK_KHR_MAINTENANCE_8_EXTENSION_NAME);
    physical_device_selector.add_required_extensions(device_extensions);

    auto physical_device_result = physical_device_selector.select();
    if (!physical_device_result) {
        auto error = physical_device_result.error();

        LOG_ERROR("Failed to select Vulkan Physical Device! {}", error.message());
        return std::unexpected(VK_ERROR_DEVICE_LOST);
    }

    self.physical_device = physical_device_result.value();

    LOG_TRACE("Created physical device.");

    VkPhysicalDeviceVulkan14Features vk14_features = {};
    vk14_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES;
    vk14_features.pushDescriptor = true;

    VkPhysicalDeviceVulkan13Features vk13_features = {};
    vk13_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    vk13_features.synchronization2 = true;
    vk13_features.shaderDemoteToHelperInvocation = true;

    VkPhysicalDeviceVulkan12Features vk12_features = {};
    vk12_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
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
    vk12_features.vulkanMemoryModel = true;
    vk12_features.vulkanMemoryModelDeviceScope = true;
    vk12_features.storageBuffer8BitAccess = true;
    vk12_features.scalarBlockLayout = true;
    vk12_features.shaderInt8 = true;
    vk12_features.shaderSubgroupExtendedTypes = true;
    vk12_features.samplerFilterMinmax = true;

    VkPhysicalDeviceVulkan11Features vk11_features = {};
    vk11_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
    vk11_features.variablePointers = true;
    vk11_features.variablePointersStorageBuffer = true;
    vk11_features.shaderDrawParameters = true;

    VkPhysicalDeviceMaintenance8FeaturesKHR maintenance_8_features = {};
    maintenance_8_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_8_FEATURES_KHR;
    maintenance_8_features.maintenance8 = true;

    VkPhysicalDeviceFeatures2 vk10_features = {};
    vk10_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    vk10_features.features.vertexPipelineStoresAndAtomics = true;
    vk10_features.features.fragmentStoresAndAtomics = true;
    vk10_features.features.shaderInt64 = true;
    vk10_features.features.multiDrawIndirect = true;
    vk10_features.features.samplerAnisotropy = true;

    VkPhysicalDeviceShaderImageAtomicInt64FeaturesEXT image_atomic_int64_features = {};
    image_atomic_int64_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_IMAGE_ATOMIC_INT64_FEATURES_EXT;
    image_atomic_int64_features.shaderImageInt64Atomics = true;

    vkb::DeviceBuilder device_builder(self.physical_device);
    device_builder //
        .add_pNext(&vk14_features)
        .add_pNext(&vk13_features)
        .add_pNext(&vk12_features)
        .add_pNext(&vk11_features)
        // Maintenance 8 allows the copy of depth images, but
        // LLVMPipe doesn't support Image<u64> on shader yet.
        // .add_pNext(&maintenance_8_features)

        // NOTE: LLVMPipe does not support this extension yet
        //.add_pNext(&image_atomic_int64_features)
        .add_pNext(&vk10_features);
    auto device_result = device_builder.build();
    if (!device_result) {
        auto error = device_result.error();
        auto vk_error = device_result.vk_result();

        LOG_ERROR("Failed to select Vulkan Device! {}", error.message());
        return std::unexpected(vk_error);
    }

    self.handle = device_result.value();

    LOG_TRACE("Created device.");

    vuk::FunctionPointers vulkan_functions = {};
    vulkan_functions.vkGetInstanceProcAddr = self.instance.fp_vkGetInstanceProcAddr;
    vulkan_functions.vkGetDeviceProcAddr = self.handle.fp_vkGetDeviceProcAddr;
    vulkan_functions.load_pfns(self.instance, self.handle, true);

    auto physical_device_properties = VkPhysicalDeviceProperties{};
    vulkan_functions.vkGetPhysicalDeviceProperties(self.physical_device, &physical_device_properties);
    self.device_limits = physical_device_properties.limits;

    std::vector<std::unique_ptr<vuk::Executor>> executors;

    auto graphics_queue = self.handle.get_queue(vkb::QueueType::graphics).value();
    auto graphics_queue_family_index = self.handle.get_queue_index(vkb::QueueType::graphics).value();
    executors.push_back(
        vuk::create_vkqueue_executor(vulkan_functions, self.handle, graphics_queue, graphics_queue_family_index, vuk::DomainFlagBits::eGraphicsQueue)
    );

    auto compute_queue = self.handle.get_queue(vkb::QueueType::compute).value();
    auto compute_queue_family_index = self.handle.get_queue_index(vkb::QueueType::compute).value();
    executors.push_back(
        vuk::create_vkqueue_executor(vulkan_functions, self.handle, compute_queue, compute_queue_family_index, vuk::DomainFlagBits::eComputeQueue)
    );

    auto transfer_queue = self.handle.get_queue(vkb::QueueType::transfer).value();
    auto transfer_queue_family_index = self.handle.get_queue_index(vkb::QueueType::transfer).value();
    executors.push_back(
        vuk::create_vkqueue_executor(vulkan_functions, self.handle, transfer_queue, transfer_queue_family_index, vuk::DomainFlagBits::eTransferQueue)
    );

    executors.push_back(std::make_unique<vuk::ThisThreadExecutor>());
    self.runtime.emplace(
        vuk::RuntimeCreateParameters{
            .instance = self.instance,
            .device = self.handle,
            .physical_device = self.physical_device,
            .executors = std::move(executors),
            .pointers = vulkan_functions,
        }
    );

    self.frame_resources.emplace(*self.runtime, frame_count);
    self.allocator.emplace(*self.frame_resources);
    self.runtime->set_shader_target_version(VK_API_VERSION_1_4);
    self.shader_compiler = SlangCompiler::create().value();

    self.transfer_manager.init(self).value();
    self.transfer_manager.acquire(self.frame_resources.value());

    LOG_INFO("Initialized device.");

    return {};
}

auto Device::destroy(this Device &self) -> void {
    ZoneScoped;

    self.frame_resources->get_next_frame();
    self.wait();

    auto destroy_resource_pool = [&self](auto &pool) -> void {
        for (auto i = 0_sz; i < pool.size(); i++) {
            auto *v = pool.slot_from_index(i);
            if (v) {
                self.allocator->deallocate({ v, 1 });
            }
        }

        pool.reset();
    };

    destroy_resource_pool(self.resources.buffers);
    destroy_resource_pool(self.resources.images);
    destroy_resource_pool(self.resources.image_views);
    self.resources.samplers.reset();
    self.resources.pipelines.reset();

    self.transfer_manager.destroy();
    self.shader_compiler.destroy();

    self.frame_resources.reset();

    vuk::current_module->collect_garbage();

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

auto Device::new_frame(this Device &self, vuk::Swapchain &swap_chain) -> vuk::Value<vuk::ImageAttachment> {
    ZoneScoped;

    if (self.transfer_manager.frame_allocator) {
        self.transfer_manager.wait_for_ops(self.compiler);
        self.transfer_manager.release();
    }

    self.transfer_manager.acquire(self.frame_resources.value());
    self.runtime->next_frame();

    auto acquired_swapchain = vuk::acquire_swapchain(swap_chain);
    auto acquired_image = vuk::acquire_next_image("present_image", std::move(acquired_swapchain));

    return acquired_image;
}

auto Device::end_frame(this Device &self, vuk::Value<vuk::ImageAttachment> &&target_attachment) -> void {
    ZoneScoped;

    auto on_begin_pass = [](void *user_data, vuk::Name pass_name, vuk::CommandBuffer &cmd_list, vuk::DomainFlagBits) {
        auto *device = static_cast<Device *>(user_data);
        auto query_it = device->pass_queries.find(pass_name);
        if (query_it == device->pass_queries.end()) {
            auto start_ts = device->runtime->create_timestamp_query();
            auto end_ts = device->runtime->create_timestamp_query();
            query_it = device->pass_queries.emplace(pass_name, ls::pair(start_ts, end_ts)).first;
        }

        auto &[start_ts, _] = query_it->second;
        cmd_list.write_timestamp(start_ts, vuk::PipelineStageFlagBits::eBottomOfPipe);

        return static_cast<void *>(&(*query_it));
    };

    auto on_end_pass = [](void *, void *pass_data, vuk::CommandBuffer &cmd_list) {
        auto *query_ptr = static_cast<std::pair<const vuk::Name, ls::pair<vuk::Query, vuk::Query>> *>(pass_data);
        auto &[start_ts, end_ts] = query_ptr->second;

        cmd_list.write_timestamp(end_ts);
    };

    auto result = vuk::enqueue_presentation(std::move(target_attachment));
    result.submit(
        *self.transfer_manager.frame_allocator,
        self.compiler,
        { .graph_label = {},
          .callbacks = {
              .on_begin_pass = on_begin_pass,
              .on_end_pass = on_end_pass,
              .user_data = &self,
          } });
}

auto Device::wait(this Device &self, LR_CALLSTACK) -> void {
    ZoneScopedN("Device Wait Idle");

    LOG_TRACE("Device wait idle triggered at {}:{}!", LOC.file_name(), LOC.line());
    self.runtime->wait_idle();
}

auto Device::create_persistent_descriptor_set(this Device &self, ls::span<BindlessDescriptorInfo> bindings, u32 index)
    -> vuk::Unique<vuk::PersistentDescriptorSet> {
    ZoneScoped;

    u32 descriptor_count = 0;
    auto raw_bindings = std::vector<VkDescriptorSetLayoutBinding>(bindings.size());
    auto binding_flags = std::vector<VkDescriptorBindingFlags>(bindings.size());
    for (const auto &[binding, raw_binding, raw_binding_flags] : std::views::zip(bindings, raw_bindings, binding_flags)) {
        raw_binding.binding = binding.binding;
        raw_binding.descriptorType = vuk::DescriptorBinding::vk_descriptor_type(binding.type);
        raw_binding.descriptorCount = binding.descriptor_count;
        raw_binding.stageFlags = VK_SHADER_STAGE_ALL;
        raw_binding_flags = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
        descriptor_count += binding.descriptor_count;
    }

    vuk::DescriptorSetLayoutCreateInfo layout_ci = {
        .index = index,
        .bindings = std::move(raw_bindings),
        .flags = std::move(binding_flags),
    };

    return self.runtime->create_persistent_descriptorset(self.allocator.value(), layout_ci, descriptor_count);
}

auto Device::commit_descriptor_set(this Device &self, vuk::PersistentDescriptorSet &set) -> void {
    ZoneScoped;

    set.commit(self.runtime.value());
}

auto Device::create_swap_chain(this Device &self, VkSurfaceKHR surface, ls::option<vuk::Swapchain> old_swap_chain)
    -> std::expected<vuk::Swapchain, vuk::VkException> {
    ZoneScoped;

    VkPresentModeKHR present_mode = self.frame_count() == 1 ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_IMMEDIATE_KHR;
    vkb::SwapchainBuilder builder(self.handle, surface);
    builder.set_desired_min_image_count(self.frame_count());
    builder.set_desired_format(
        vuk::SurfaceFormatKHR{
            .format = vuk::Format::eR8G8B8A8Srgb,
            .colorSpace = vuk::ColorSpaceKHR::eSrgbNonlinear,
        }
    );
    builder.add_fallback_format(
        vuk::SurfaceFormatKHR{
            .format = vuk::Format::eB8G8R8A8Srgb,
            .colorSpace = vuk::ColorSpaceKHR::eSrgbNonlinear,
        }
    );
    builder.set_desired_present_mode(present_mode);
    builder.set_image_usage_flags(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);

    auto recycling = false;
    auto result = vkb::Result<vkb::Swapchain>{ vkb::Swapchain{} };
    if (!old_swap_chain) {
        result = builder.build();
        old_swap_chain.emplace(self.allocator.value(), result->image_count);
    } else {
        recycling = true;
        builder.set_old_swapchain(old_swap_chain->swapchain);
        result = builder.build();
    }

    if (!result.has_value()) {
        auto error = result.error();
        auto vk_result = result.vk_result();
        LOG_ERROR("Failed to create Swap Chain! {}", error.message());

        return std::unexpected(vk_result);
    }

    if (recycling) {
        self.allocator->deallocate(std::span{ &old_swap_chain->swapchain, 1 });
        for (auto &v : old_swap_chain->images) {
            self.allocator->deallocate(std::span{ &v.image_view, 1 });
        }
    }

    old_swap_chain->images.clear();

    auto images = *result->get_images();
    auto image_views = *result->get_image_views();

    for (u32 i = 0; i < images.size(); i++) {
        vuk::ImageAttachment attachment = {
            .image = vuk::Image{ .image = images[i], .allocation = nullptr },
            .image_view = vuk::ImageView{ { 0 }, image_views[i] },
            .extent = { .width = result->extent.width, .height = result->extent.height, .depth = 1 },
            .format = static_cast<vuk::Format>(result->image_format),
            .sample_count = vuk::Samples::e1,
            .view_type = vuk::ImageViewType::e2D,
            .components = {},
            .base_level = 0,
            .level_count = 1,
            .base_layer = 0,
            .layer_count = 1,
        };
        old_swap_chain->images.push_back(attachment);
    }

    old_swap_chain->swapchain = result->swapchain;
    old_swap_chain->surface = surface;

    return std::move(*old_swap_chain);
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

auto Device::buffer(this Device &self, BufferID id) -> ls::option<vuk::Buffer> {
    ZoneScoped;

    return self.resources.buffers.slot_clone(id);
}

auto Device::image(this Device &self, ImageID id) -> ls::option<vuk::Image> {
    ZoneScoped;

    return self.resources.images.slot_clone(id);
}

auto Device::image_view(this Device &self, ImageViewID id) -> ls::option<vuk::ImageView> {
    ZoneScoped;

    return self.resources.image_views.slot_clone(id);
}

auto Device::sampler(this Device &self, SamplerID id) -> ls::option<vuk::Sampler> {
    ZoneScoped;

    return self.resources.samplers.slot_clone(id);
}

auto Device::pipeline(this Device &self, PipelineID id) -> vuk::PipelineBaseInfo ** {
    ZoneScoped;

    return self.resources.pipelines.slot(id);
}

auto Device::destroy(this Device &self, BufferID id) -> void {
    ZoneScoped;

    auto *buffer = self.resources.buffers.slot(id);
    self.allocator->deallocate({ buffer, 1 });

    self.resources.buffers.destroy_slot(id);
}

auto Device::destroy(this Device &self, ImageID id) -> void {
    ZoneScoped;

    auto *image = self.resources.images.slot(id);
    self.allocator->deallocate({ image, 1 });

    self.resources.images.destroy_slot(id);
}

auto Device::destroy(this Device &self, ImageViewID id) -> void {
    ZoneScoped;

    auto *image_view = self.resources.image_views.slot(id);
    self.allocator->deallocate({ image_view, 1 });

    self.resources.image_views.destroy_slot(id);
}

auto Device::destroy(this Device &self, SamplerID id) -> void {
    ZoneScoped;

    self.resources.samplers.destroy_slot(id);
}

auto Device::destroy(this Device &self, PipelineID id) -> void {
    ZoneScoped;

    self.resources.pipelines.destroy_slot(id);
}

} // namespace lr
