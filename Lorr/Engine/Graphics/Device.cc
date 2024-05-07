#include "Device.hh"

#include "Shader.hh"

#include "Memory/Bit.hh"
#include "Memory/Stack.hh"

namespace lr::graphics {
VKResult Device::init(vkb::Instance *instance)
{
    ZoneScoped;

    m_instance = instance;

    {
        vkb::PhysicalDeviceSelector physical_device_selector(*instance);
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

        m_physical_device = physical_device_result.value();
    }

    {
        /// DEVICE INITIALIZATION ///
        // if
        // (selected_physical_device.enable_extension_if_present("VK_EXT_descriptor_buffer"))
        //     features |= DeviceFeature::DescriptorBuffer;
        if (m_physical_device.enable_extension_if_present("VK_EXT_memory_budget"))
            m_supported_features |= DeviceFeature::MemoryBudget;

        vkb::DeviceBuilder device_builder(m_physical_device);

        VkPhysicalDeviceDescriptorBufferFeaturesEXT desciptor_buffer_features = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT,
            .pNext = nullptr,
            .descriptorBuffer = true,
            .descriptorBufferCaptureReplay = false,
            .descriptorBufferImageLayoutIgnored = true,
            .descriptorBufferPushDescriptors = true,
        };
        if (m_supported_features & DeviceFeature::DescriptorBuffer)
            device_builder.add_pNext(&desciptor_buffer_features);

        auto device_result = device_builder.build();
        if (!device_result) {
            LR_LOG_ERROR("Failed to select Vulkan Device!");
            return static_cast<VKResult>(device_result.vk_result());
        }

        m_handle = device_result.value();
    }

    if (!vulkan::load_device(m_handle, instance->fp_vkGetDeviceProcAddr)) {
        LR_LOG_ERROR("Failed to create Vulkan Device! Extension not found.");
        return VKResult::ExtNotPresent;
    }

    set_object_name_raw<VK_OBJECT_TYPE_INSTANCE>(m_instance->instance, "Instance");
    set_object_name_raw<VK_OBJECT_TYPE_DEVICE>(m_handle.device, "Device");
    set_object_name_raw<VK_OBJECT_TYPE_PHYSICAL_DEVICE>(m_physical_device.physical_device, "Physical Device");

    auto create_queue_impl = [this](CommandQueue &target, vkb::QueueType type, std::string_view name) {
        create_timeline_semaphores({ &target.m_semaphore, 1 }, 0);
        target.m_handle = m_handle.get_queue(type).value();
        target.m_index = m_handle.get_queue_index(type).value();

        set_object_name(target, name);
    };

    m_queues = Unique<std::array<CommandQueue, usize(CommandType::Count)>>(this);
    create_queue_impl(m_queues->at(usize(CommandType::Graphics)), vkb::QueueType::graphics, "Graphics Queue");
    create_queue_impl(m_queues->at(usize(CommandType::Compute)), vkb::QueueType::compute, "Compute Queue");
    create_queue_impl(m_queues->at(usize(CommandType::Transfer)), vkb::QueueType::transfer, "Transfer Queue");

    /// ALLOCATOR INITIALIZATION ///

    VmaVulkanFunctions vulkan_functions = {};
    vulkan_functions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
    vulkan_functions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

    VmaAllocatorCreateInfo allocator_create_info = {
        .flags = VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT,
        .physicalDevice = m_physical_device,
        .device = m_handle,
        .preferredLargeHeapBlockSize = 0,
        .pAllocationCallbacks = nullptr,
        .pDeviceMemoryCallbacks = nullptr,
        .pHeapSizeLimit = nullptr,
        .pVulkanFunctions = &vulkan_functions,
        .instance = *m_instance,
        .vulkanApiVersion = VK_API_VERSION_1_3,
        .pTypeExternalMemoryHandleTypes = nullptr,
    };

    auto result = CHECK(vmaCreateAllocator(&allocator_create_info, &m_allocator));
    if (!result) {
        LR_LOG_ERROR("Failed to create VKAllocator! {}", (u32)result);
        return result;
    }

    /// DEVICE CONTEXT ///
    DescriptorBindingFlag bindless_flags = DescriptorBindingFlag::UpdateAfterBind | DescriptorBindingFlag::PartiallyBound;
    ls::static_vector<DescriptorPoolSize, 4> pool_sizes;
    ls::static_vector<DescriptorSetLayoutElement, 4> layout_elements;
    ls::static_vector<DescriptorBindingFlag, 4> binding_flags;

    /// SAMPLER ///
    u32 descriptor_count_sampler = m_resources.samplers.max_resources();
    pool_sizes.push_back({
        .type = DescriptorType::Sampler,
        .count = descriptor_count_sampler,
    });
    layout_elements.push_back({
        .binding = LR_DESCRIPTOR_INDEX_SAMPLER,
        .descriptor_type = DescriptorType::Sampler,
        .descriptor_count = descriptor_count_sampler,
        .stage = ShaderStageFlag::All,
    });
    binding_flags.push_back(bindless_flags);

    /// SAMPLED IMAGE ///
    u32 descriptor_count_image = m_resources.images.max_resources();
    pool_sizes.push_back({
        .type = DescriptorType::SampledImage,
        .count = descriptor_count_image,
    });
    layout_elements.push_back({
        .binding = LR_DESCRIPTOR_INDEX_IMAGES,
        .descriptor_type = DescriptorType::SampledImage,
        .descriptor_count = descriptor_count_image,
        .stage = ShaderStageFlag::All,
    });
    binding_flags.push_back(bindless_flags);

    /// STORAGE IMAGE ///
    pool_sizes.push_back({
        .type = DescriptorType::StorageImage,
        .count = descriptor_count_image,
    });
    layout_elements.push_back({
        .binding = LR_DESCRIPTOR_INDEX_STORAGE_IMAGES,
        .descriptor_type = DescriptorType::StorageImage,
        .descriptor_count = descriptor_count_image,
        .stage = ShaderStageFlag::All,
    });
    binding_flags.push_back(bindless_flags);

    /// STORAGE BUFFER - BDA ///
    u32 descriptor_count_buffer = m_resources.buffers.max_resources();
    pool_sizes.push_back({
        .type = DescriptorType::StorageBuffer,
        .count = descriptor_count_buffer,
    });
    layout_elements.push_back({
        .binding = LR_DESCRIPTOR_INDEX_BUFFERS,
        .descriptor_type = DescriptorType::StorageBuffer,
        .descriptor_count = descriptor_count_buffer,
        .stage = ShaderStageFlag::All,
    });
    binding_flags.push_back(bindless_flags);

    DescriptorPoolInfo pool_info = {
        .sizes = pool_sizes,
        .max_sets = 1,
        .flags = DescriptorPoolFlag::UpdateAfterBind,
    };
    auto [descriptor_pool, result_pool] = create_descriptor_pool(pool_info);
    m_descriptor_pool = std::move(descriptor_pool);
    if (!result_pool) {
        return result_pool;
    }
    set_object_name(*m_descriptor_pool, "Bindless Descriptor Pool");

    DescriptorSetLayoutInfo layout_info = {
        .elements = layout_elements,
        .binding_flags = binding_flags,
        .flags = DescriptorSetLayoutFlag::UpdateAfterBindPool,
    };
    auto [descriptor_set_layout, result_layout] = create_descriptor_set_layout(layout_info);
    m_descriptor_set_layout = std::move(descriptor_set_layout);
    if (!result_layout) {
        return result_layout;
    }
    set_object_name(*m_descriptor_set_layout, "Bindless Descriptor Set Layout");

    DescriptorSetInfo set_info = {
        .layout = *m_descriptor_set_layout,
        .pool = *m_descriptor_pool,
    };
    auto [descriptor_set, result_set] = create_descriptor_set(set_info);
    m_descriptor_set = std::move(descriptor_set);
    if (!result_set) {
        return result_set;
    }
    set_object_name(*m_descriptor_set, "Bindless Descriptor Set");

    auto &pipeline_layouts = m_resources.pipeline_layouts;
    for (u32 i = 0; i < pipeline_layouts.size(); i++) {
        PushConstantRange push_constant_range = {
            .stage = ShaderStageFlag::All,
            .offset = 0,
            .size = static_cast<u32>(i * sizeof(u32)),
        };
        PipelineLayoutInfo pipeline_layout_info = {
            .layouts = { &*m_descriptor_set_layout, 1 },
            .push_constants = { &push_constant_range, 1 },
        };

        if (i == 0) {
            pipeline_layout_info.push_constants = {};
        }

        create_pipeline_layouts({ &pipeline_layouts[i], 1 }, pipeline_layout_info);
    }

    return VKResult::Success;
}

VKResult Device::create_command_allocators(std::span<CommandAllocator> allocators, const CommandAllocatorInfo &info)
{
    ZoneScoped;

    VkCommandPoolCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = static_cast<VkCommandPoolCreateFlags>(info.flags),
        .queueFamilyIndex = m_queues->at(usize(info.type)).m_index,
    };

    VKResult result = VKResult::Success;
    for (CommandAllocator &allocator : allocators) {
        result = CHECK(vkCreateCommandPool(m_handle, &create_info, nullptr, &allocator.m_handle));
        if (result != VKResult::Success) {
            LR_LOG_ERROR("Failed to create Command Allocator! {}", result);
            return result;
        }
    }

    return VKResult::Success;
}

void Device::delete_command_allocators(std::span<CommandAllocator> allocators)
{
    ZoneScoped;

    for (CommandAllocator &allocator : allocators) {
        vkDestroyCommandPool(m_handle, allocator, nullptr);
    }
}

VKResult Device::create_command_lists(std::span<CommandList> lists, CommandAllocator &command_allocator)
{
    ZoneScoped;

    VkCommandBufferAllocateInfo allocate_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = command_allocator,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };

    VKResult result = VKResult::Success;
    for (CommandList &list : lists) {
        result = CHECK(vkAllocateCommandBuffers(m_handle, &allocate_info, &list.m_handle));
        if (result != VKResult::Success) {
            LR_LOG_ERROR("Failed to create Command List! {}", result);
            return result;
        }

        list.m_allocator = &command_allocator;
        list.m_device = this;
    }

    return VKResult::Success;
}

void Device::delete_command_lists(std::span<CommandList> lists)
{
    ZoneScoped;

    for (CommandList &list : lists) {
        vkFreeCommandBuffers(m_handle, *list.m_allocator, 1, &list.m_handle);
    }
}

void Device::begin_command_list(CommandList &list)
{
    ZoneScoped;

    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr,
    };
    vkBeginCommandBuffer(list, &begin_info);
}

void Device::end_command_list(CommandList &list)
{
    ZoneScoped;

    vkEndCommandBuffer(list);
}

void Device::reset_command_allocator(CommandAllocator &allocator)
{
    ZoneScoped;

    vkResetCommandPool(m_handle, allocator, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT);
}

VKResult Device::create_binary_semaphores(std::span<Semaphore> semaphores)
{
    ZoneScoped;

    VkSemaphoreTypeCreateInfo semaphore_type_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
        .pNext = nullptr,
        .semaphoreType = VK_SEMAPHORE_TYPE_BINARY,
        .initialValue = 0,
    };

    VkSemaphoreCreateInfo semaphore_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = &semaphore_type_info,
        .flags = 0,
    };

    VKResult result = VKResult::Success;
    for (Semaphore &semaphore : semaphores) {
        result = CHECK(vkCreateSemaphore(m_handle, &semaphore_info, nullptr, &semaphore.m_handle));

        if (result != VKResult::Success) {
            return result;
        }
    }

    return result;
}

VKResult Device::create_timeline_semaphores(std::span<Semaphore> semaphores, u64 initial_value)
{
    ZoneScoped;

    VkSemaphoreTypeCreateInfo semaphore_type_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
        .pNext = nullptr,
        .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
        .initialValue = initial_value,
    };

    VkSemaphoreCreateInfo semaphore_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = &semaphore_type_info,
        .flags = 0,
    };

    VKResult result = VKResult::Success;
    for (Semaphore &semaphore : semaphores) {
        semaphore.m_counter = initial_value;
        result = CHECK(vkCreateSemaphore(m_handle, &semaphore_info, nullptr, &semaphore.m_handle));

        if (result != VKResult::Success) {
            return result;
        }
    }

    return result;
}

void Device::delete_semaphores(std::span<Semaphore> semaphores)
{
    ZoneScoped;

    for (Semaphore &semaphore : semaphores) {
        vkDestroySemaphore(m_handle, semaphore, nullptr);
    }
}

VKResult Device::wait_for_semaphore(Semaphore &semaphore, u64 desired_value, u64 timeout)
{
    ZoneScoped;

    VkSemaphoreWaitInfo wait_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
        .pNext = nullptr,
        .flags = 0,
        .semaphoreCount = 1,
        .pSemaphores = &semaphore.m_handle,
        .pValues = &desired_value,
    };
    return CHECK(vkWaitSemaphores(m_handle, &wait_info, timeout));
}

Result<u64, VKResult> Device::get_semaphore_counter(Semaphore &semaphore)
{
    u64 value = 0;
    auto result = CHECK(vkGetSemaphoreCounterValue(m_handle, semaphore, &value));

    return Result(value, result);
}

VKResult Device::create_swap_chain(SwapChain &swap_chain, const SwapChainInfo &info)
{
    ZoneScoped;

    // make sure all work on sc images finish before destroying it
    wait_for_work();

    vkb::SwapchainBuilder builder(m_handle, info.surface);
    builder.set_desired_min_image_count((u32)info.buffering);
    builder.set_desired_extent(info.extent.width, info.extent.height);
    builder.set_desired_format({ VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR });
    builder.set_image_usage_flags(
        static_cast<VkImageUsageFlags>(ImageUsage::ColorAttachment | ImageUsage::Sampled | ImageUsage::TransferSrc));
    auto result = builder.build();
    if (!result) {
        LR_LOG_ERROR("Failed to create SwapChain!");
        return static_cast<VKResult>(result.vk_result());
    }

    swap_chain.m_format = static_cast<Format>(result->image_format);
    swap_chain.m_extent = { result->extent.width, result->extent.height };
    swap_chain.m_image_count = result->image_count;

    swap_chain.m_frame_sema = Unique<Semaphore>(this);
    swap_chain.m_acquire_semas = Unique<ls::static_vector<Semaphore, Limits::FrameCount>>(this);
    swap_chain.m_present_semas = Unique<ls::static_vector<Semaphore, Limits::FrameCount>>(this);
    swap_chain.m_acquire_semas->resize(swap_chain.m_image_count);
    swap_chain.m_present_semas->resize(swap_chain.m_image_count);
    create_timeline_semaphores({ &*swap_chain.m_frame_sema, 1 }, 0);
    create_binary_semaphores(*swap_chain.m_acquire_semas);
    create_binary_semaphores(*swap_chain.m_present_semas);
    swap_chain.m_handle = result.value();
    swap_chain.m_surface = info.surface;
    swap_chain.m_frame_sema.set_name("SwapChain frame counter Semaphore");

    u32 i = 0;
    for (Semaphore &v : *swap_chain.m_acquire_semas) {
        set_object_name(v, fmt::format("SwapChain Acquire Sema {}", i++));
    }

    i = 0;
    for (Semaphore &v : *swap_chain.m_present_semas) {
        set_object_name(v, fmt::format("SwapChain Present Sema {}", i++));
    }

    set_object_name_raw<VK_OBJECT_TYPE_SWAPCHAIN_KHR>(swap_chain.m_handle.swapchain, "SwapChain");
    return VKResult::Success;
}

void Device::delete_swap_chains(std::span<SwapChain> swap_chains)
{
    ZoneScoped;

    wait_for_work();

    for (auto &swap_chain : swap_chains) {
        vkb::destroy_swapchain(swap_chain);
    }
}

VKResult Device::get_swapchain_images(SwapChain &swap_chain, std::span<ImageID> images)
{
    ZoneScoped;
    memory::ScopedStack stack;

    u32 image_count = images.size();
    auto image_ids = stack.alloc<ImageID>(image_count);
    auto images_raw = stack.alloc<VkImage>(image_count);

    auto result = CHECK(vkGetSwapchainImagesKHR(m_handle, swap_chain.m_handle, &image_count, images_raw.data()));
    if (!result) {
        LR_LOG_ERROR("Failed to get SwapChain images! {}", result);
        return result;
    }

    for (u32 i = 0; i < images.size(); i++) {
        VkImage &image_handle = images_raw[i];
        Extent3D extent = { swap_chain.m_extent.width, swap_chain.m_extent.height, 1 };
        auto [image, image_id] = m_resources.images.create(image_handle, nullptr, swap_chain.m_format, extent, 1, 1);

        images[i] = image_id;
    }

    return VKResult::Success;
}

void Device::wait_for_work()
{
    ZoneScoped;

    vkDeviceWaitIdle(m_handle);
}

Result<u32, VKResult> Device::acquire_next_image(SwapChain &swap_chain, Semaphore &acquire_sema)
{
    ZoneScoped;

    auto &frame_sema = swap_chain.frame_sema();
    wait_for_semaphore(frame_sema, frame_sema.counter());

    u32 image_id = 0;
    auto result = CHECK(vkAcquireNextImageKHR(m_handle, swap_chain.m_handle, UINT64_MAX, acquire_sema, nullptr, &image_id));
    if (result != VKResult::Success && result != VKResult::Suboptimal) {
        return result;
    }

    return Result(image_id, result);
}

void Device::collect_garbage()
{
    ZoneScoped;

    u64 current_garbage_counter = get_semaphore_counter(get_queue(CommandType::Graphics).semaphore());
    auto checker_fn = [&](auto &queue, const auto &deleter_fn) {
        for (auto i = queue.begin(); i != queue.end();) {
            auto &[v, timeline_val] = *i;
            if (current_garbage_counter > timeline_val) {
                deleter_fn({ &v, 1 });
                i = queue.erase(i);
            }
            else {
                i++;
            }
        }
    };

    checker_fn(m_garbage_command_lists, [this](std::span<CommandList> s) { delete_command_lists(s); });
}

VKResult Device::create_pipeline_layouts(std::span<PipelineLayout> pipeline_layouts, const PipelineLayoutInfo &info)
{
    ZoneScoped;
    memory::ScopedStack stack;

    auto vk_pipeline_layouts = stack.alloc<VkDescriptorSetLayout>(info.layouts.size());
    for (u32 i = 0; i < vk_pipeline_layouts.size(); i++) {
        vk_pipeline_layouts[i] = info.layouts[i];
    }

    for (auto &pipeline_layout : pipeline_layouts) {
        VkPipelineLayoutCreateInfo pipeline_layout_create_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .setLayoutCount = static_cast<u32>(info.layouts.size()),
            .pSetLayouts = vk_pipeline_layouts.data(),
            .pushConstantRangeCount = static_cast<u32>(info.push_constants.size()),
            .pPushConstantRanges = reinterpret_cast<const VkPushConstantRange *>(info.push_constants.data()),
        };

        for (auto &layout : pipeline_layouts) {
            VkPipelineLayout layout_handle = VK_NULL_HANDLE;
            auto result = CHECK(vkCreatePipelineLayout(m_handle, &pipeline_layout_create_info, nullptr, &layout_handle));
            if (result != VKResult::Success) {
                LR_LOG_ERROR("Failed to create Pipeline Layout! {}", result);
                return result;
            }

            layout.m_handle = layout_handle;
        }
    }

    return VKResult::Success;
}

void Device::delete_pipeline_layouts(std::span<PipelineLayout> pipeline_layout)
{
    ZoneScoped;

    for (auto &layout : pipeline_layout) {
        vkDestroyPipelineLayout(m_handle, layout, nullptr);
    }
}

Result<PipelineID, VKResult> Device::create_graphics_pipeline(const GraphicsPipelineInfo &info)
{
    ZoneScoped;
    memory::ScopedStack stack;

    VkPipelineRenderingCreateInfo rendering_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .pNext = nullptr,
        .viewMask = 0,
        .colorAttachmentCount = static_cast<u32>(info.color_attachment_formats.size()),
        .pColorAttachmentFormats = reinterpret_cast<const VkFormat *>(info.color_attachment_formats.data()),
        .depthAttachmentFormat = static_cast<VkFormat>(info.depth_attachment_format),
        .stencilAttachmentFormat = static_cast<VkFormat>(info.stencil_attachment_format),
    };

    // Viewport State

    VkPipelineViewportStateCreateInfo viewport_state_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .viewportCount = static_cast<u32>(info.viewports.size()),
        .pViewports = reinterpret_cast<const VkViewport *>(info.viewports.data()),
        .scissorCount = static_cast<u32>(info.scissors.size()),
        .pScissors = reinterpret_cast<const VkRect2D *>(info.scissors.data()),
    };

    // Shaders

    auto vk_shader_stage_infos = stack.alloc<VkPipelineShaderStageCreateInfo>(info.shader_ids.size());
    for (u32 i = 0; i < vk_shader_stage_infos.size(); i++) {
        auto &vk_info = vk_shader_stage_infos[i];
        auto &v = info.shader_ids[i];
        Shader *shader = get_shader(v);

        vk_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vk_info.pNext = nullptr;
        vk_info.flags = 0;
        vk_info.stage = static_cast<VkShaderStageFlagBits>(shader->m_stage);
        vk_info.module = shader->m_handle;
        vk_info.pName = "main";
        vk_info.pSpecializationInfo = nullptr;
    }

    // Input Layout

    auto vk_vertex_binding_infos = stack.alloc<VkVertexInputBindingDescription>(info.vertex_binding_infos.size());
    for (u32 i = 0; i < vk_vertex_binding_infos.size(); i++) {
        auto &vk_info = vk_vertex_binding_infos[i];
        auto &v = info.vertex_binding_infos[i];

        vk_info.binding = v.binding;
        vk_info.stride = v.stride;
        vk_info.inputRate = static_cast<VkVertexInputRate>(v.input_rate);
    }

    auto vk_vertex_attrib_infos = stack.alloc<VkVertexInputAttributeDescription>(info.vertex_attrib_infos.size());
    for (u32 i = 0; i < vk_vertex_attrib_infos.size(); i++) {
        auto &vk_info = vk_vertex_attrib_infos[i];
        auto &v = info.vertex_attrib_infos[i];

        vk_info.format = static_cast<VkFormat>(v.format);
        vk_info.location = v.location;
        vk_info.binding = v.binding;
        vk_info.offset = v.offset;
    }

    VkPipelineVertexInputStateCreateInfo input_layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .vertexBindingDescriptionCount = static_cast<u32>(vk_vertex_binding_infos.size()),
        .pVertexBindingDescriptions = vk_vertex_binding_infos.data(),
        .vertexAttributeDescriptionCount = static_cast<u32>(vk_vertex_attrib_infos.size()),
        .pVertexAttributeDescriptions = vk_vertex_attrib_infos.data(),
    };

    // Input Assembly

    VkPipelineInputAssemblyStateCreateInfo input_assembly_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .topology = static_cast<VkPrimitiveTopology>(info.primitive_type),
        .primitiveRestartEnable = false,
    };

    // Tessellation

    VkPipelineTessellationStateCreateInfo tessellation_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .patchControlPoints = 0,  // TODO: Tessellation
    };

    // Rasterizer

    VkPipelineRasterizationStateCreateInfo rasterizer_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .depthClampEnable = info.enable_depth_clamp,
        .rasterizerDiscardEnable = false,
        .polygonMode = info.enable_wireframe ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL,
        .cullMode = static_cast<VkCullModeFlags>(info.cull_mode),
        .frontFace = static_cast<VkFrontFace>(!info.front_face_ccw),
        .depthBiasEnable = info.enable_depth_bias,
        .depthBiasConstantFactor = info.depth_bias_factor,
        .depthBiasClamp = info.depth_bias_clamp,
        .depthBiasSlopeFactor = info.depth_slope_factor,
        .lineWidth = info.line_width,
    };

    // Multisampling

    VkPipelineMultisampleStateCreateInfo multisample_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .rasterizationSamples = static_cast<VkSampleCountFlagBits>(1 << (info.multisample_bit_count - 1)),
        .sampleShadingEnable = false,
        .minSampleShading = 0,
        .pSampleMask = nullptr,
        .alphaToCoverageEnable = info.enable_alpha_to_coverage,
        .alphaToOneEnable = info.enable_alpha_to_one,
    };

    // Depth Stencil

    VkPipelineDepthStencilStateCreateInfo depth_stencil_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .depthTestEnable = info.enable_depth_test,
        .depthWriteEnable = info.enable_depth_write,
        .depthCompareOp = static_cast<VkCompareOp>(info.depth_compare_op),
        .depthBoundsTestEnable = info.enable_depth_bounds_test,
        .stencilTestEnable = info.enable_stencil_test,
        .front = info.stencil_front_face_op,
        .back = info.stencil_back_face_op,
        .minDepthBounds = 0,
        .maxDepthBounds = 0,
    };

    // Color Blending

    VkPipelineColorBlendStateCreateInfo color_blend_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .logicOpEnable = false,
        .logicOp = {},
        .attachmentCount = static_cast<u32>(info.blend_attachments.size()),
        .pAttachments = reinterpret_cast<const VkPipelineColorBlendAttachmentState *>(info.blend_attachments.data()),
        .blendConstants = { info.blend_constants.x, info.blend_constants.y, info.blend_constants.z, info.blend_constants.w },
    };

    /// DYNAMIC STATE ------------------------------------------------------------

    constexpr static VkDynamicState k_dynamic_states[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
        VK_DYNAMIC_STATE_LINE_WIDTH,
        VK_DYNAMIC_STATE_DEPTH_BIAS,
        VK_DYNAMIC_STATE_BLEND_CONSTANTS,
        VK_DYNAMIC_STATE_DEPTH_BOUNDS,
        VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK,
        VK_DYNAMIC_STATE_STENCIL_WRITE_MASK,
        VK_DYNAMIC_STATE_STENCIL_REFERENCE,
        VK_DYNAMIC_STATE_CULL_MODE,
        VK_DYNAMIC_STATE_FRONT_FACE,
        VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY,
        VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT,
        VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT,
        VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE,
        VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE,
        VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE,
        VK_DYNAMIC_STATE_DEPTH_COMPARE_OP,
        VK_DYNAMIC_STATE_DEPTH_BOUNDS_TEST_ENABLE,
        VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE,
        VK_DYNAMIC_STATE_STENCIL_OP,
        VK_DYNAMIC_STATE_RASTERIZER_DISCARD_ENABLE,
        VK_DYNAMIC_STATE_DEPTH_BIAS_ENABLE,
        VK_DYNAMIC_STATE_PRIMITIVE_RESTART_ENABLE,
    };
    static_assert(count_of(k_dynamic_states) == usize(DynamicState::Count));

    u32 dynamic_state_mask = static_cast<u32>(info.dynamic_state);
    usize state_count = memory::set_bit_count(dynamic_state_mask);
    ls::static_vector<VkDynamicState, usize(DynamicState::Count)> dynamic_states;
    for (usize i = 0; i < state_count; i++) {
        u32 shift = memory::find_first_set32(dynamic_state_mask);
        dynamic_states.push_back(k_dynamic_states[shift]);
        dynamic_state_mask ^= 1 << shift;
    }

    VkPipelineDynamicStateCreateInfo dynamic_state_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .dynamicStateCount = static_cast<u32>(dynamic_states.size()),
        .pDynamicStates = dynamic_states.data(),
    };

    /// GRAPHICS PIPELINE --------------------------------------------------------

    VkPipelineCreateFlags pipeline_create_flags = 0;
    if (is_feature_supported(DeviceFeature::DescriptorBuffer))
        pipeline_create_flags |= VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

    PipelineLayout *pipeline_layout = info.layout;
    if (pipeline_layout == nullptr) {
        pipeline_layout = &m_resources.pipeline_layouts[0];
    }

    VkGraphicsPipelineCreateInfo pipeline_create_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &rendering_info,
        .flags = pipeline_create_flags,
        .stageCount = static_cast<u32>(vk_shader_stage_infos.size()),
        .pStages = vk_shader_stage_infos.data(),
        .pVertexInputState = &input_layout_info,
        .pInputAssemblyState = &input_assembly_info,
        .pTessellationState = &tessellation_info,
        .pViewportState = reinterpret_cast<const VkPipelineViewportStateCreateInfo *>(&viewport_state_info),
        .pRasterizationState = &rasterizer_info,
        .pMultisampleState = &multisample_info,
        .pDepthStencilState = &depth_stencil_info,
        .pColorBlendState = &color_blend_info,
        .pDynamicState = &dynamic_state_info,
        .layout = *pipeline_layout,
        .renderPass = nullptr,
        .subpass = 0,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = 0,
    };

    VkPipeline pipeline_handle = VK_NULL_HANDLE;
    auto result = CHECK(vkCreateGraphicsPipelines(m_handle, nullptr, 1, &pipeline_create_info, nullptr, &pipeline_handle));
    if (result != VKResult::Success) {
        LR_LOG_ERROR("Failed to create Graphics Pipeline! {}", result);
        return result;
    }

    auto pipeline = m_resources.pipelines.create(pipeline_handle, PipelineBindPoint::Graphics);
    if (!pipeline) {
        LR_LOG_ERROR("Failed to allocate Graphics Pipeline!");
        return VKResult::OutOfPoolMem;
    }

    return pipeline.id;
};

Result<PipelineID, VKResult> Device::create_compute_pipeline(const ComputePipelineInfo &info)
{
    ZoneScoped;

    VkComputePipelineCreateInfo pipeline_create_info = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .stage = info.compute_shader_info,
        .layout = info.layout,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = 0,
    };

    VkPipeline pipeline_handle = VK_NULL_HANDLE;
    auto result = CHECK(vkCreateComputePipelines(m_handle, nullptr, 1, &pipeline_create_info, nullptr, &pipeline_handle));
    if (result != VKResult::Success) {
        LR_LOG_ERROR("Failed to create Compute Pipeline! {}", result);
        return result;
    }

    auto pipeline = m_resources.pipelines.create(pipeline_handle, PipelineBindPoint::Compute);
    if (!pipeline) {
        LR_LOG_ERROR("Failed to allocate Compute Pipeline!");
        return VKResult::OutOfPoolMem;
    }

    return pipeline.id;
}

void Device::delete_pipelines(std::span<PipelineID> pipelines)
{
    ZoneScoped;

    for (PipelineID &pipeline_id : pipelines) {
        Pipeline *pipeline = m_resources.pipelines.get(pipeline_id);
        if (!pipeline) {
            LR_LOG_ERROR("Referencing to invalid Pipeline.");
            return;
        }

        vkDestroyPipeline(m_handle, *pipeline, nullptr);
        m_resources.pipelines.destroy(pipeline_id);
    }
}

Result<ShaderID, VKResult> Device::create_shader(ShaderStageFlag stage, std::span<u32> ir)
{
    ZoneScoped;

    VkShaderModuleCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .codeSize = ir.size_bytes(),
        .pCode = ir.data(),
    };

    VkShaderModule shader_module = VK_NULL_HANDLE;
    auto result = CHECK(vkCreateShaderModule(m_handle, &create_info, nullptr, &shader_module));
    if (result != VKResult::Success) {
        LR_LOG_ERROR("Failed to create shader! {}", result);
        return result;
    }

    auto shader = m_resources.shaders.create(shader_module, stage);
    if (!shader) {
        LR_LOG_ERROR("Failed to allocate shader!");
        return VKResult::OutOfPoolMem;
    }

    return shader.id;
}

void Device::delete_shaders(std::span<ShaderID> shaders)
{
    ZoneScoped;

    for (ShaderID &shader_id : shaders) {
        Shader *shader = m_resources.shaders.get(shader_id);
        if (!shader) {
            LR_LOG_ERROR("Referencing to invalid Shader.");
            return;
        }

        vkDestroyShaderModule(m_handle, *shader, nullptr);
        m_resources.shaders.destroy(shader_id);
    }
}

UniqueResult<DescriptorPool> Device::create_descriptor_pool(const DescriptorPoolInfo &info)
{
    ZoneScoped;

    Unique<DescriptorPool> descriptor_pool(this);
    VkDescriptorPoolCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = static_cast<VkDescriptorPoolCreateFlags>(info.flags),
        .maxSets = info.max_sets,
        .poolSizeCount = static_cast<u32>(info.sizes.size()),
        .pPoolSizes = reinterpret_cast<const VkDescriptorPoolSize *>(info.sizes.data()),
    };

    VkDescriptorPool pool_handle = VK_NULL_HANDLE;
    auto result = CHECK(vkCreateDescriptorPool(m_handle, &create_info, nullptr, &pool_handle));
    if (result != VKResult::Success) {
        LR_LOG_ERROR("Failed to create Descriptor Pool! {}", result);
        return result;
    }

    descriptor_pool.reset(DescriptorPool(pool_handle, info.flags));
    return descriptor_pool;
}

void Device::delete_descriptor_pools(std::span<DescriptorPool> descriptor_pools)
{
    ZoneScoped;

    for (auto &pool : descriptor_pools) {
        vkDestroyDescriptorPool(m_handle, pool, nullptr);
    }
}

UniqueResult<DescriptorSetLayout> Device::create_descriptor_set_layout(const DescriptorSetLayoutInfo &info)
{
    ZoneScoped;

    LR_ASSERT(info.elements.size() == info.binding_flags.size());
    Unique<DescriptorSetLayout> layout(this);

    VkDescriptorSetLayoutBindingFlagsCreateInfo binding_flags_create_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
        .pNext = nullptr,
        .bindingCount = static_cast<u32>(info.binding_flags.size()),
        .pBindingFlags = reinterpret_cast<VkDescriptorBindingFlags *>(info.binding_flags.data()),
    };

    VkDescriptorSetLayoutCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = &binding_flags_create_info,
        .flags = static_cast<VkDescriptorSetLayoutCreateFlags>(info.flags),
        .bindingCount = static_cast<u32>(info.elements.size()),
        .pBindings = reinterpret_cast<DescriptorSetLayoutElement::VkType *>(info.elements.data()),
    };

    VkDescriptorSetLayout layout_handle = VK_NULL_HANDLE;
    auto result = CHECK(vkCreateDescriptorSetLayout(m_handle, &create_info, nullptr, &layout_handle));
    if (result != VKResult::Success) {
        LR_LOG_ERROR("Failed to create Descriptor Set Layout! {}", result);
        return result;
    }

    layout.reset(DescriptorSetLayout(layout_handle));
    return layout;
}

void Device::delete_descriptor_set_layouts(std::span<DescriptorSetLayout> layouts)
{
    ZoneScoped;

    for (auto &layout : layouts) {
        vkDestroyDescriptorSetLayout(m_handle, layout, nullptr);
    }
}

UniqueResult<DescriptorSet> Device::create_descriptor_set(const DescriptorSetInfo &info)
{
    ZoneScoped;

    Unique<DescriptorSet> descriptor_set(this);
    VkDescriptorSetVariableDescriptorCountAllocateInfo set_count_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO,
        .pNext = nullptr,
        .descriptorSetCount = 1,
        .pDescriptorCounts = &info.descriptor_count,
    };

    VkDescriptorSetAllocateInfo allocate_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = &set_count_info,
        .descriptorPool = info.pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &info.layout.m_handle,
    };

    VkDescriptorSet descriptor_set_handle = VK_NULL_HANDLE;
    auto result = CHECK(vkAllocateDescriptorSets(m_handle, &allocate_info, &descriptor_set_handle));
    if (result != VKResult::Success) {
        LR_LOG_ERROR("Failed to create Descriptor Set! {}", result);
        return result;
    }

    descriptor_set.reset(DescriptorSet(descriptor_set_handle, &info.pool));
    return descriptor_set;
}

void Device::delete_descriptor_sets(std::span<DescriptorSet> descriptor_sets)
{
    ZoneScoped;

    for (auto &set : descriptor_sets) {
        DescriptorPool *pool = set.m_pool;
        if (pool->m_flags & DescriptorPoolFlag::FreeDescriptorSet) {
            continue;
        }

        vkFreeDescriptorSets(m_handle, *pool, 1, &set.m_handle);
    }
}

void Device::update_descriptor_set(std::span<WriteDescriptorSet> writes, std::span<CopyDescriptorSet> copies)
{
    ZoneScoped;

    vkUpdateDescriptorSets(
        m_handle,
        writes.size(),
        reinterpret_cast<const VkWriteDescriptorSet *>(writes.data()),
        copies.size(),
        reinterpret_cast<const VkCopyDescriptorSet *>(copies.data()));
}

Result<BufferID, VKResult> Device::create_buffer(const BufferInfo &info)
{
    ZoneScoped;

    BufferUsage buffer_usage = info.usage_flags | BufferUsage::BufferDeviceAddress;
    VkBufferCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .size = info.data_size,
        .usage = static_cast<VkBufferUsageFlags>(buffer_usage),
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,
    };

    VmaAllocationCreateInfo allocation_info = {
        .flags = 0,
        .usage = VMA_MEMORY_USAGE_AUTO,
        .requiredFlags = 0,
        .preferredFlags = 0,
        .memoryTypeBits = 0,
        .pool = nullptr,
        .pUserData = nullptr,
        .priority = 0.0f,
    };

    VkBuffer buffer_handle = nullptr;
    VmaAllocation allocation = nullptr;
    auto result = CHECK(vmaCreateBuffer(m_allocator, &create_info, &allocation_info, &buffer_handle, &allocation, nullptr));
    if (result != VKResult::Success) {
        LR_LOG_ERROR("Failed to create Buffer! {}", result);
        return result;
    }

    auto buffer = m_resources.buffers.create(buffer_handle, allocation);
    if (!buffer) {
        LR_LOG_ERROR("Failed to allocate Buffer!");
        return VKResult::OutOfPoolMem;
    }

    BufferDescriptorInfo descriptor_info = { .buffer = *buffer.resource };
    WriteDescriptorSet write_set_info = {
        .dst_descriptor_set = *m_descriptor_set,
        .dst_binding = LR_DESCRIPTOR_INDEX_BUFFERS,
        .dst_element = static_cast<u32>(buffer.id),
        .count = 1,
        .type = DescriptorType::Sampler,
        .buffer_info = &descriptor_info,
    };
    update_descriptor_set({ &write_set_info, 1 }, {});

    return buffer.id;
}

void Device::delete_buffers(std::span<BufferID> buffers)
{
    ZoneScoped;

    for (BufferID &buffer_id : buffers) {
        Buffer *buffer = m_resources.buffers.get(buffer_id);
        if (!buffer) {
            LR_LOG_ERROR("Referencing to invalid Buffer!");
            return;
        }

        vmaDestroyBuffer(m_allocator, *buffer, buffer->m_allocation);
        m_resources.buffers.destroy(buffer_id);
    }
}

Result<ImageID, VKResult> Device::create_image(const ImageInfo &info)
{
    ZoneScoped;

    VkImageCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .imageType = static_cast<VkImageType>(info.type),
        .format = static_cast<VkFormat>(info.format),
        .extent = info.extent,
        .mipLevels = info.mip_levels,
        .arrayLayers = info.slice_count,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = static_cast<VkImageUsageFlags>(info.usage_flags),
        .sharingMode = VK_SHARING_MODE_CONCURRENT,
        .queueFamilyIndexCount = static_cast<u32>(info.queue_indices.size()),
        .pQueueFamilyIndices = info.queue_indices.data(),
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    VmaAllocationCreateInfo allocation_info = {
        .flags = 0,
        .usage = VMA_MEMORY_USAGE_AUTO,
        .requiredFlags = 0,
        .preferredFlags = 0,
        .memoryTypeBits = 0,
        .pool = nullptr,
        .pUserData = nullptr,
        .priority = 0.0f,
    };

    VkImage image_handle = nullptr;
    VmaAllocation allocation = nullptr;

    auto result = CHECK(vmaCreateImage(m_allocator, &create_info, &allocation_info, &image_handle, &allocation, nullptr));
    if (result != VKResult::Success) {
        LR_LOG_ERROR("Failed to create Image! {}", result);
        return result;
    }

    auto image = m_resources.images.create(image_handle, allocation, info.format, info.extent, info.slice_count, info.mip_levels);
    if (!image) {
        LR_LOG_ERROR("Failed to allocate Image!");
        return VKResult::OutOfPoolMem;
    }

    return image.id;
}

void Device::delete_images(std::span<ImageID> images)
{
    ZoneScoped;

    for (ImageID &image_id : images) {
        Image *image = m_resources.images.get(image_id);
        if (!image) {
            LR_LOG_ERROR("Referencing to invalid Image!");
            return;
        }

        vmaDestroyImage(m_allocator, *image, image->m_allocation);
        m_resources.images.destroy(image_id);
    }
}

Result<ImageViewID, VKResult> Device::create_image_view(const ImageViewInfo &info)
{
    ZoneScoped;

    Image *image = get_image(info.image_id);
    VkImageViewCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .image = *image,
        .viewType = static_cast<VkImageViewType>(info.type),
        .format = static_cast<VkFormat>(image->m_format),
        .components = { static_cast<VkComponentSwizzle>(info.swizzle_r),
                        static_cast<VkComponentSwizzle>(info.swizzle_g),
                        static_cast<VkComponentSwizzle>(info.swizzle_b),
                        static_cast<VkComponentSwizzle>(info.swizzle_a) },
        .subresourceRange = info.subresource_range,
    };

    VkImageView image_view_handle = nullptr;
    auto result = CHECK(vkCreateImageView(m_handle, &create_info, nullptr, &image_view_handle));
    if (result != VKResult::Success) {
        LR_LOG_ERROR("Failed to create Image View! {}", result);
        return result;
    }

    auto image_view = m_resources.image_views.create(image_view_handle, image->m_format, info.type, info.subresource_range);
    if (!image_view) {
        LR_LOG_ERROR("Failed to allocate Image View!");
        return VKResult::OutOfPoolMem;
    }

    // TODO: Copy paste this once again for storage images
    // but with layout is set to GENERAL and type as StorageImage
    ImageDescriptorInfo descriptor_info = {
        .image_view = *image_view.resource,
        .image_layout = ImageLayout::ColorReadOnly,
    };
    WriteDescriptorSet write_set_info = {
        .dst_descriptor_set = *m_descriptor_set,
        .dst_binding = LR_DESCRIPTOR_INDEX_IMAGES,
        .dst_element = static_cast<u32>(image_view.id),
        .count = 1,
        .type = DescriptorType::SampledImage,
        .image_info = &descriptor_info,
    };
    update_descriptor_set({ &write_set_info, 1 }, {});

    return image_view.id;
}

void Device::delete_image_views(std::span<ImageViewID> image_views)
{
    ZoneScoped;

    for (ImageViewID &image_view_id : image_views) {
        ImageView *image_view = m_resources.image_views.get(image_view_id);
        if (!image_view) {
            LR_LOG_ERROR("Referencing to invalid Image View!");
            return;
        }

        vkDestroyImageView(m_handle, image_view->m_handle, nullptr);
    }
}

Result<SamplerID, VKResult> Device::create_sampler(const SamplerInfo &info)
{
    ZoneScoped;

    VkSamplerCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .magFilter = static_cast<VkFilter>(info.min_filter),
        .minFilter = static_cast<VkFilter>(info.min_filter),
        .mipmapMode = static_cast<VkSamplerMipmapMode>(info.mip_filter),
        .addressModeU = static_cast<VkSamplerAddressMode>(info.address_u),
        .addressModeV = static_cast<VkSamplerAddressMode>(info.address_v),
        .addressModeW = static_cast<VkSamplerAddressMode>(info.address_w),
        .mipLodBias = info.mip_lod_bias,
        .anisotropyEnable = info.use_anisotropy,
        .maxAnisotropy = info.max_anisotropy,
        .compareEnable = info.compare_op != CompareOp::Never,
        .compareOp = static_cast<VkCompareOp>(info.compare_op),
        .minLod = info.min_lod,
        .maxLod = info.max_lod,
        .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK,
        .unnormalizedCoordinates = 0,
    };

    VkSampler sampler_handle = nullptr;
    auto result = CHECK(vkCreateSampler(m_handle, &create_info, nullptr, &sampler_handle));
    if (result != VKResult::Success) {
        LR_LOG_ERROR("Failed to create Sampler! {}", result);
        return result;
    }

    auto sampler = m_resources.samplers.create(sampler_handle);
    if (!sampler) {
        LR_LOG_ERROR("Failed to allocate Sampler!");
        return VKResult::OutOfPoolMem;
    }

    ImageDescriptorInfo descriptor_info = { .sampler = *sampler.resource };
    WriteDescriptorSet write_set_info = {
        .dst_descriptor_set = *m_descriptor_set,
        .dst_binding = LR_DESCRIPTOR_INDEX_SAMPLER,
        .dst_element = static_cast<u32>(sampler.id),
        .count = 1,
        .type = DescriptorType::Sampler,
        .image_info = &descriptor_info,
    };
    update_descriptor_set({ &write_set_info, 1 }, {});

    return sampler.id;
}

void Device::delete_samplers(std::span<SamplerID> samplers)
{
    ZoneScoped;

    for (SamplerID &sampler_id : samplers) {
        Sampler *sampler = m_resources.samplers.get(sampler_id);
        if (!sampler) {
            LR_LOG_ERROR("Referencing to invalid Sampler!");
            return;
        }

        vkDestroySampler(m_handle, *sampler, nullptr);
    }
}

}  // namespace lr::graphics
