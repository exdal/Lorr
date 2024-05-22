#include "Device.hh"

#include "Resource.hh"
#include "Shader.hh"

#include "Engine/Memory/Bit.hh"
#include "Engine/Memory/Stack.hh"

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
        vk12_features.hostQueryReset = true;
        physical_device_selector.set_required_features_12(vk12_features);

        VkPhysicalDeviceFeatures vk10_features = {};
        vk10_features.vertexPipelineStoresAndAtomics = true;
        vk10_features.fragmentStoresAndAtomics = true;
        vk10_features.shaderInt64 = true;
        physical_device_selector.set_required_features(vk10_features);
        physical_device_selector.add_required_extensions({
            "VK_KHR_swapchain",
#if TRACY_ENABLE
            "VK_KHR_calibrated_timestamps",
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
        if (m_physical_device.properties.limits.timestampPeriod != 0) {
            m_supported_features |= DeviceFeature::QueryTimestamp;
        }

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

    {
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

        create_queue_impl(get_queue(CommandType::Graphics), vkb::QueueType::graphics, "Graphics Queue");
        create_queue_impl(get_queue(CommandType::Compute), vkb::QueueType::compute, "Compute Queue");
        create_queue_impl(get_queue(CommandType::Transfer), vkb::QueueType::transfer, "Transfer Queue");
    }

    /// ALLOCATOR INITIALIZATION ///
    {
        VmaVulkanFunctions vulkan_functions = {};
        vulkan_functions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
        vulkan_functions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

        VmaAllocatorCreateInfo allocator_create_info = {
            .flags = VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT | VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
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
    }

    /// DEVICE CONTEXT ///
    {
        DescriptorBindingFlag bindless_flags = DescriptorBindingFlag::UpdateAfterBind | DescriptorBindingFlag::PartiallyBound;
        std::vector<DescriptorPoolSize> pool_sizes;
        std::vector<DescriptorSetLayoutElement> layout_elements;
        std::vector<DescriptorBindingFlag> binding_flags;
        auto add_descriptor_binding = [&](u32 binding, DescriptorType type, u32 count) {
            pool_sizes.push_back({
                .type = type,
                .count = count,
            });
            layout_elements.push_back({
                .binding = binding,
                .descriptor_type = type,
                .descriptor_count = count,
                .stage = ShaderStageFlag::All,
            });
            binding_flags.push_back(bindless_flags);
        };

        add_descriptor_binding(LR_DESCRIPTOR_INDEX_SAMPLER, DescriptorType::Sampler, m_resources.samplers.max_resources());
        add_descriptor_binding(LR_DESCRIPTOR_INDEX_IMAGES, DescriptorType::SampledImage, m_resources.images.max_resources());
        add_descriptor_binding(LR_DESCRIPTOR_INDEX_STORAGE_IMAGES, DescriptorType::StorageImage, m_resources.image_views.max_resources());
        add_descriptor_binding(LR_DESCRIPTOR_INDEX_STORAGE_BUFFERS, DescriptorType::StorageBuffer, m_resources.buffers.max_resources());
        add_descriptor_binding(LR_DESCRIPTOR_INDEX_BDA_ARRAY, DescriptorType::StorageBuffer, 1);

        DescriptorPoolFlag descriptor_pool_flags = DescriptorPoolFlag::UpdateAfterBind;
        if (auto result = create_descriptor_pools({ &m_descriptor_pool, 1 }, { .sizes = pool_sizes, .max_sets = 1, .flags = descriptor_pool_flags }); !result) {
            LR_LOG_ERROR("Failed to init device! Descriptor pool failed '{}'.", result);
            return result;
        }
        set_object_name(m_descriptor_pool, "Bindless Descriptor Pool");

        DescriptorSetLayoutFlag descriptor_set_layout_flags = DescriptorSetLayoutFlag::UpdateAfterBindPool;
        if (auto result = create_descriptor_set_layouts(
                { &m_descriptor_set_layout, 1 }, { .elements = layout_elements, .binding_flags = binding_flags, .flags = descriptor_set_layout_flags });
            !result) {
            LR_LOG_ERROR("Failed to init device! Descriptor set layout failed '{}'", result);
            return result;
        }
        set_object_name(m_descriptor_set_layout, "Bindless Descriptor Set Layout");

        if (auto result = create_descriptor_sets({ &m_descriptor_set, 1 }, { .layout = m_descriptor_set_layout, .pool = m_descriptor_pool }); !result) {
            LR_LOG_ERROR("Failed to init device! Descriptor set failed '{}'.", result);
            return result;
        }
        set_object_name(m_descriptor_set, "Bindless Descriptor Set");

        auto &pipeline_layouts = m_resources.pipeline_layouts;
        for (u32 i = 0; i < pipeline_layouts.size(); i++) {
            PushConstantRange push_constant_range = {
                .stage = ShaderStageFlag::All,
                .offset = 0,
                .size = static_cast<u32>(i * sizeof(u32)),
            };
            PipelineLayoutInfo pipeline_layout_info = {
                .layouts = { &m_descriptor_set_layout, 1 },
                .push_constants = { &push_constant_range, !!i },
            };

            create_pipeline_layouts({ &pipeline_layouts[i], 1 }, pipeline_layout_info);
            set_object_name(pipeline_layouts[i], fmt::format("Pipeline Layout ({})", i));
        }
    }

    {
        m_bda_array_buffer = create_buffer({
            .usage_flags = BufferUsage::Storage,
            .flags = MemoryFlag::HostSeqWrite | MemoryFlag::Dedicated,
            .preference = MemoryPreference::Device,
            .data_size = m_resources.buffers.max_resources() * sizeof(u64),
        });
        Buffer *bda_array_buffer = get_buffer(m_bda_array_buffer);
        vmaMapMemory(m_allocator, bda_array_buffer->m_allocation, reinterpret_cast<void **>(&m_bda_array_host_addr));

        BufferDescriptorInfo buffer_descriptor_info = { .buffer = *bda_array_buffer, .offset = 0, .range = VK_WHOLE_SIZE };
        WriteDescriptorSet write_info = {
            .dst_descriptor_set = m_descriptor_set,
            .dst_binding = LR_DESCRIPTOR_INDEX_BDA_ARRAY,
            .dst_element = 0,
            .count = 1,
            .type = DescriptorType::StorageBuffer,
            .buffer_info = &buffer_descriptor_info,
        };
        update_descriptor_sets({ &write_info, 1 }, {});
    }

    create_timeline_semaphores({ &m_garbage_timeline_sema, 1 }, 0);

    return VKResult::Success;
}

VKResult Device::create_timestamp_query_pools(std::span<TimestampQueryPool> query_pools, const TimestampQueryPoolInfo &info)
{
    ZoneScoped;

    VkQueryPoolCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .queryType = VK_QUERY_TYPE_TIMESTAMP,
        .queryCount = info.query_count,
        .pipelineStatistics = {},
    };

    for (TimestampQueryPool &query_pool : query_pools) {
        VKResult result = CHECK(vkCreateQueryPool(m_handle, &create_info, nullptr, &query_pool.m_handle));
        if (result != VKResult::Success) {
            LR_LOG_ERROR("Failed to create timestamp query pool! {}", result);
            return result;
        }

        // This is literally just memset to zero, cmd version is same but happens when queue is on execute state
        vkResetQueryPool(m_handle, query_pool, 0, info.query_count);
    }

    return VKResult::Success;
}

void Device::delete_timestamp_query_pools(std::span<const TimestampQueryPool> query_pools)
{
    ZoneScoped;

    for (const TimestampQueryPool &query_pool : query_pools) {
        vkDestroyQueryPool(m_handle, query_pool, nullptr);
    }
}

void Device::defer(std::span<const TimestampQueryPool> query_pools)
{
    ZoneScoped;

    for (const TimestampQueryPool &query_pool : query_pools) {
        m_garbage_query_pools.emplace(query_pool, m_garbage_timeline_sema.counter());
    }
}

void Device::get_timestamp_query_pool_results(TimestampQueryPool &query_pool, u32 first_query, u32 count, std::span<u64> time_stamps)
{
    ZoneScoped;

    vkGetQueryPoolResults(
        m_handle,
        query_pool,
        first_query,
        count,
        time_stamps.size() * sizeof(u64),
        time_stamps.data(),
        2 * sizeof(u64),
        VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);
}

VKResult Device::create_command_allocators(std::span<CommandAllocator> command_allocators, const CommandAllocatorInfo &info)
{
    ZoneScoped;

    VkCommandPoolCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = static_cast<VkCommandPoolCreateFlags>(info.flags),
        .queueFamilyIndex = get_queue(info.type).family_index(),
    };

    for (CommandAllocator &allocator : command_allocators) {
        VKResult result = CHECK(vkCreateCommandPool(m_handle, &create_info, nullptr, &allocator.m_handle));
        if (result != VKResult::Success) {
            LR_LOG_ERROR("Failed to create Command Allocator! {}", result);
            return result;
        }

        allocator.m_type = info.type;
        allocator.m_queue = &get_queue(info.type);
        set_object_name(allocator, info.debug_name);
    }

    return VKResult::Success;
}

void Device::delete_command_allocators(std::span<const CommandAllocator> command_allocators)
{
    ZoneScoped;

    for (const CommandAllocator &allocator : command_allocators) {
        vkDestroyCommandPool(m_handle, allocator, nullptr);
    }
}

void Device::defer(std::span<const CommandAllocator> command_allocators)
{
    ZoneScoped;

    for (const CommandAllocator &command_allocator : command_allocators) {
        command_allocator.m_queue->defer({ { command_allocator } });
    }
}

VKResult Device::create_command_lists(std::span<CommandList> command_lists, CommandAllocator &command_allocator)
{
    ZoneScoped;

    VkCommandBufferAllocateInfo allocate_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = command_allocator,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };

    for (CommandList &list : command_lists) {
        VKResult result = CHECK(vkAllocateCommandBuffers(m_handle, &allocate_info, &list.m_handle));
        if (result != VKResult::Success) {
            LR_LOG_ERROR("Failed to create Command List! {}", result);
            return result;
        }

        list.m_type = command_allocator.m_type;
        list.m_allocator = command_allocator.m_handle;
        list.m_device = this;
    }

    return VKResult::Success;
}

void Device::delete_command_lists(std::span<const CommandList> command_lists)
{
    ZoneScoped;

    for (const CommandList &list : command_lists) {
        vkFreeCommandBuffers(m_handle, list.m_allocator, 1, &list.m_handle);
    }
}

void Device::defer(std::span<const CommandList> command_lists)
{
    ZoneScoped;

    for (const CommandList &command_list : command_lists) {
        CommandQueue &queue = get_queue(command_list.m_type);
        queue.defer({ { command_list } });
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

    for (Semaphore &semaphore : semaphores) {
        VKResult result = CHECK(vkCreateSemaphore(m_handle, &semaphore_info, nullptr, &semaphore.m_handle));
        if (result != VKResult::Success) {
            LR_LOG_ERROR("Failed to create binary semaphore! {}", result);
            return result;
        }
    }

    return VKResult::Success;
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

    for (Semaphore &semaphore : semaphores) {
        semaphore.m_counter = initial_value;
        VKResult result = CHECK(vkCreateSemaphore(m_handle, &semaphore_info, nullptr, &semaphore.m_handle));
        if (result != VKResult::Success) {
            LR_LOG_ERROR("Failed to create timeline semaphore! {}", result);
            return result;
        }
    }

    return VKResult::Success;
}

void Device::delete_semaphores(std::span<const Semaphore> semaphores)
{
    ZoneScoped;

    for (const Semaphore &semaphore : semaphores) {
        vkDestroySemaphore(m_handle, semaphore, nullptr);
    }
}

void Device::defer(std::span<const Semaphore> semaphores)
{
    ZoneScoped;

    for (const Semaphore &semaphore : semaphores) {
        m_garbage_semaphores.emplace(semaphore, m_garbage_timeline_sema.counter());
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

    wait_for_work();

    vkb::SwapchainBuilder builder(m_handle, info.surface);
    builder.set_desired_min_image_count((u32)info.buffering);
    builder.set_desired_extent(info.extent.width, info.extent.height);
    builder.set_desired_format({ VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR });
    builder.set_desired_present_mode(VK_PRESENT_MODE_IMMEDIATE_KHR);
    builder.set_image_usage_flags(static_cast<VkImageUsageFlags>(ImageUsage::ColorAttachment | ImageUsage::TransferDst));
    auto result = builder.build();

    if (!result) {
        LR_LOG_ERROR("Failed to create SwapChain!");
        return static_cast<VKResult>(result.vk_result());
    }

    swap_chain = {};
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

void Device::delete_swap_chains(std::span<const SwapChain> swap_chains)
{
    ZoneScoped;

    wait_for_work();

    for (const SwapChain &swap_chain : swap_chains) {
        vkb::destroy_swapchain(swap_chain.m_handle);
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
    auto result = static_cast<VKResult>(vkAcquireNextImageKHR(m_handle, swap_chain.m_handle, UINT64_MAX, acquire_sema, nullptr, &image_id));
    if (result != VKResult::Success && result != VKResult::Suboptimal) {
        return result;
    }

    return Result(image_id, result);
}

void Device::collect_garbage()
{
    ZoneScoped;

    auto checkndelete_fn = [&](auto &container, u64 sema_counter, const auto &deleter_fn) {
        for (auto i = container.begin(); i != container.end();) {
            auto &[v, timeline_val] = *i;
            if (sema_counter > timeline_val) {
                deleter_fn({ &v, 1 });
                i = container.erase(i);
            }
            else {
                i++;
            }
        }
    };

    for (CommandQueue &v : m_queues) {
        u64 queue_counter = get_semaphore_counter(v.semaphore());
        checkndelete_fn(v.m_garbage_samplers, queue_counter, [this](std::span<SamplerID> s) { delete_samplers(s); });
        checkndelete_fn(v.m_garbage_image_views, queue_counter, [this](std::span<ImageViewID> s) { delete_image_views(s); });
        checkndelete_fn(v.m_garbage_images, queue_counter, [this](std::span<ImageID> s) { delete_images(s); });
        checkndelete_fn(v.m_garbage_buffers, queue_counter, [this](std::span<BufferID> s) { delete_buffers(s); });
        checkndelete_fn(v.m_garbage_command_lists, queue_counter, [this](std::span<CommandList> s) { delete_command_lists(s); });
        checkndelete_fn(v.m_garbage_command_allocators, queue_counter, [this](std::span<CommandAllocator> s) { delete_command_allocators(s); });
    }

    u64 device_counter = get_semaphore_counter(m_garbage_timeline_sema);
    checkndelete_fn(m_garbage_semaphores, device_counter, [this](std::span<Semaphore> s) { delete_semaphores(s); });
    checkndelete_fn(m_garbage_query_pools, device_counter, [this](std::span<TimestampQueryPool> s) { delete_timestamp_query_pools(s); });
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

void Device::delete_pipeline_layouts(std::span<const PipelineLayout> pipeline_layout)
{
    ZoneScoped;

    for (const PipelineLayout &layout : pipeline_layout) {
        vkDestroyPipelineLayout(m_handle, layout.m_handle, nullptr);
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

    Shader *shader = get_shader(info.shader_id);
    PipelineShaderStageInfo shader_info = {
        .shader_stage = shader->m_stage,
        .module = shader->m_handle,
        .entry_point = "main",
    };

    PipelineLayout *pipeline_layout = info.layout;
    if (pipeline_layout == nullptr) {
        pipeline_layout = &m_resources.pipeline_layouts[0];
    }

    VkComputePipelineCreateInfo pipeline_create_info = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .stage = shader_info,
        .layout = *pipeline_layout,
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

void Device::delete_pipelines(std::span<const PipelineID> pipelines)
{
    ZoneScoped;

    for (const PipelineID pipeline_id : pipelines) {
        Pipeline *pipeline = m_resources.pipelines.get(pipeline_id);
        if (!pipeline) {
            LR_LOG_ERROR("Referencing to invalid Pipeline.");
            return;
        }

        vkDestroyPipeline(m_handle, *pipeline, nullptr);
        m_resources.pipelines.destroy(pipeline_id);
        pipeline->m_handle = VK_NULL_HANDLE;
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

void Device::delete_shaders(std::span<const ShaderID> shaders)
{
    ZoneScoped;

    for (const ShaderID shader_id : shaders) {
        Shader *shader = m_resources.shaders.get(shader_id);
        if (!shader) {
            LR_LOG_ERROR("Referencing to invalid Shader.");
            return;
        }

        vkDestroyShaderModule(m_handle, *shader, nullptr);
        m_resources.shaders.destroy(shader_id);
        shader->m_handle = VK_NULL_HANDLE;
    }
}

VKResult Device::create_descriptor_pools(std::span<DescriptorPool> descriptor_pools, const DescriptorPoolInfo &info)
{
    ZoneScoped;

    VkDescriptorPoolCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = static_cast<VkDescriptorPoolCreateFlags>(info.flags),
        .maxSets = info.max_sets,
        .poolSizeCount = static_cast<u32>(info.sizes.size()),
        .pPoolSizes = reinterpret_cast<const VkDescriptorPoolSize *>(info.sizes.data()),
    };

    for (DescriptorPool &descriptor_pool : descriptor_pools) {
        VkDescriptorPool pool_handle = VK_NULL_HANDLE;
        auto result = CHECK(vkCreateDescriptorPool(m_handle, &create_info, nullptr, &pool_handle));
        if (result != VKResult::Success) {
            LR_LOG_ERROR("Failed to create Descriptor Pool! {}", result);
            return result;
        }

        descriptor_pool = DescriptorPool(pool_handle, info.flags);
    }

    return VKResult::Success;
}

void Device::delete_descriptor_pools(std::span<const DescriptorPool> descriptor_pools)
{
    ZoneScoped;

    for (const DescriptorPool &pool : descriptor_pools) {
        vkDestroyDescriptorPool(m_handle, pool.m_handle, nullptr);
    }
}

VKResult Device::create_descriptor_set_layouts(std::span<DescriptorSetLayout> descriptor_set_layouts, const DescriptorSetLayoutInfo &info)
{
    ZoneScoped;

    LR_ASSERT(info.elements.size() == info.binding_flags.size());
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

    for (DescriptorSetLayout &descriptor_set_layout : descriptor_set_layouts) {
        VkDescriptorSetLayout layout_handle = VK_NULL_HANDLE;
        auto result = CHECK(vkCreateDescriptorSetLayout(m_handle, &create_info, nullptr, &layout_handle));
        if (result != VKResult::Success) {
            LR_LOG_ERROR("Failed to create Descriptor Set Layout! {}", result);
            return result;
        }

        descriptor_set_layout = DescriptorSetLayout(layout_handle);
    }

    return VKResult::Success;
}

void Device::delete_descriptor_set_layouts(std::span<const DescriptorSetLayout> layouts)
{
    ZoneScoped;

    for (const DescriptorSetLayout &layout : layouts) {
        vkDestroyDescriptorSetLayout(m_handle, layout.m_handle, nullptr);
    }
}

VKResult Device::create_descriptor_sets(std::span<DescriptorSet> descriptor_sets, const DescriptorSetInfo &info)
{
    ZoneScoped;

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

    for (DescriptorSet &descriptor_set : descriptor_sets) {
        VkDescriptorSet descriptor_set_handle = VK_NULL_HANDLE;
        auto result = CHECK(vkAllocateDescriptorSets(m_handle, &allocate_info, &descriptor_set_handle));
        if (result != VKResult::Success) {
            LR_LOG_ERROR("Failed to create Descriptor Set! {}", result);
            return result;
        }

        descriptor_set = DescriptorSet(descriptor_set_handle, &info.pool);
    }

    return VKResult::Success;
}

void Device::delete_descriptor_sets(std::span<const DescriptorSet> descriptor_sets)
{
    ZoneScoped;

    for (const DescriptorSet &descriptor_set : descriptor_sets) {
        DescriptorPool *pool = descriptor_set.m_pool;
        if (pool->m_flags & DescriptorPoolFlag::FreeDescriptorSet) {
            continue;
        }

        vkFreeDescriptorSets(m_handle, *pool, 1, &descriptor_set.m_handle);
    }
}

void Device::update_descriptor_sets(std::span<const WriteDescriptorSet> writes, std::span<const CopyDescriptorSet> copies)
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

    constexpr static MemoryFlag host_flags = MemoryFlag::HostSeqWrite | MemoryFlag::HostReadWrite;

    VmaAllocationCreateFlags vma_creation_flags = static_cast<VmaAllocationCreateFlags>(info.flags);
    if (info.flags & host_flags) {
        vma_creation_flags |= VMA_ALLOCATION_CREATE_MAPPED_BIT;
    }
    VkBufferUsageFlags buffer_usage_flags = static_cast<VkBufferUsageFlags>(info.usage_flags | BufferUsage::BufferDeviceAddress);
    VkBufferCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .size = info.data_size,
        .usage = buffer_usage_flags,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,
    };

    VmaAllocationCreateInfo allocation_info = {
        .flags = vma_creation_flags,
        .usage = static_cast<VmaMemoryUsage>(info.preference),
        .requiredFlags = 0,
        .preferredFlags = 0,
        .memoryTypeBits = ~0u,
        .pool = nullptr,
        .pUserData = nullptr,
        .priority = 1.0f,
    };

    VkBuffer buffer_handle = nullptr;
    VmaAllocation allocation = nullptr;
    VmaAllocationInfo allocation_result = {};
    auto result = CHECK(vmaCreateBuffer(m_allocator, &create_info, &allocation_info, &buffer_handle, &allocation, &allocation_result));
    if (result != VKResult::Success) {
        LR_LOG_ERROR("Failed to create Buffer! {}", result);
        return result;
    }

    VkBufferDeviceAddressInfo device_address_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .pNext = nullptr,
        .buffer = buffer_handle,
    };
    u64 device_address = vkGetBufferDeviceAddress(m_handle, &device_address_info);

    auto buffer = m_resources.buffers.create(buffer_handle, info.data_size, allocation_result.pMappedData, device_address, allocation);
    if (!buffer) {
        LR_LOG_ERROR("Failed to allocate Buffer!");
        return VKResult::OutOfPoolMem;
    }

    if (m_bda_array_host_addr) {
        m_bda_array_host_addr[usize(buffer.id)] = device_address;
    }

    if (!info.debug_name.empty()) {
        set_object_name(*buffer.resource, info.debug_name);
    }

    return buffer.id;
}

void Device::delete_buffers(std::span<const BufferID> buffers)
{
    ZoneScoped;

    for (const BufferID buffer_id : buffers) {
        Buffer *buffer = m_resources.buffers.get(buffer_id);
        if (!buffer) {
            LR_LOG_ERROR("Referencing to invalid Buffer!");
            return;
        }

        vmaDestroyBuffer(m_allocator, *buffer, buffer->m_allocation);
        m_resources.buffers.destroy(buffer_id);
        buffer->m_handle = VK_NULL_HANDLE;
    }
}

Result<ImageID, VKResult> Device::create_image(const ImageInfo &info)
{
    ZoneScoped;

    VkSharingMode sharing_mode = VK_SHARING_MODE_EXCLUSIVE;
    if (info.queue_indices.size() > 0) {
        sharing_mode = VK_SHARING_MODE_CONCURRENT;
    }

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
        .sharingMode = sharing_mode,
        .queueFamilyIndexCount = static_cast<u32>(info.queue_indices.size()),
        .pQueueFamilyIndices = info.queue_indices.data(),
        .initialLayout = static_cast<VkImageLayout>(info.initial_layout),
    };

    VmaAllocationCreateInfo allocation_info = {
        .flags = 0,
        .usage = VMA_MEMORY_USAGE_GPU_ONLY,
        .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        .preferredFlags = 0,
        .memoryTypeBits = 0,
        .pool = nullptr,
        .pUserData = nullptr,
        .priority = 0.5f,
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

    if (!info.debug_name.empty()) {
        set_object_name(*image.resource, info.debug_name);
    }

    return image.id;
}

void Device::delete_images(std::span<const ImageID> images)
{
    ZoneScoped;

    for (const ImageID image_id : images) {
        Image *image = m_resources.images.get(image_id);
        if (!image) {
            LR_LOG_ERROR("Referencing to invalid Image!");
            return;
        }

        if (image->m_allocation) {
            // if set to falst, we are most likely destroying sc images
            vmaDestroyImage(m_allocator, *image, image->m_allocation);
        }
        m_resources.images.destroy(image_id);
        image->m_handle = VK_NULL_HANDLE;
    }
}

Result<ImageViewID, VKResult> Device::create_image_view(const ImageViewInfo &info)
{
    ZoneScoped;

    LR_ASSERT(info.usage_flags != ImageUsage::None);

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

    ls::static_vector<WriteDescriptorSet, 2> descriptor_writes = {};
    ImageDescriptorInfo sampled_descriptor_info = { .image_view = *image_view.resource, .image_layout = ImageLayout::ColorReadOnly };
    ImageDescriptorInfo storage_descriptor_info = { .image_view = *image_view.resource, .image_layout = ImageLayout::General };

    WriteDescriptorSet sampled_write_set_info = {
        .dst_descriptor_set = m_descriptor_set,
        .dst_binding = LR_DESCRIPTOR_INDEX_IMAGES,
        .dst_element = static_cast<u32>(image_view.id),
        .count = 1,
        .type = DescriptorType::SampledImage,
        .image_info = &sampled_descriptor_info,
    };
    WriteDescriptorSet storage_write_set_info = {
        .dst_descriptor_set = m_descriptor_set,
        .dst_binding = LR_DESCRIPTOR_INDEX_STORAGE_IMAGES,
        .dst_element = static_cast<u32>(image_view.id),
        .count = 1,
        .type = DescriptorType::StorageImage,
        .image_info = &storage_descriptor_info,
    };
    if (info.usage_flags & ImageUsage::Sampled) {
        descriptor_writes.push_back(sampled_write_set_info);
    }
    if (info.usage_flags & ImageUsage::Storage) {
        descriptor_writes.push_back(storage_write_set_info);
    }

    update_descriptor_sets(descriptor_writes, {});

    if (!info.debug_name.empty()) {
        set_object_name(*image_view.resource, info.debug_name);
    }

    return image_view.id;
}

void Device::delete_image_views(std::span<const ImageViewID> image_views)
{
    ZoneScoped;

    for (const ImageViewID image_view_id : image_views) {
        ImageView *image_view = m_resources.image_views.get(image_view_id);
        if (!image_view) {
            LR_LOG_ERROR("Referencing to invalid Image View!");
            return;
        }

        vkDestroyImageView(m_handle, image_view->m_handle, nullptr);
        m_resources.image_views.destroy(image_view_id);
        image_view->m_handle = VK_NULL_HANDLE;
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
        .dst_descriptor_set = m_descriptor_set,
        .dst_binding = LR_DESCRIPTOR_INDEX_SAMPLER,
        .dst_element = static_cast<u32>(sampler.id),
        .count = 1,
        .type = DescriptorType::Sampler,
        .image_info = &descriptor_info,
    };
    update_descriptor_sets({ &write_set_info, 1 }, {});

    if (!info.debug_name.empty()) {
        set_object_name(*sampler.resource, info.debug_name);
    }

    return sampler.id;
}

void Device::delete_samplers(std::span<const SamplerID> samplers)
{
    ZoneScoped;

    for (const SamplerID sampler_id : samplers) {
        Sampler *sampler = m_resources.samplers.get(sampler_id);
        if (!sampler) {
            LR_LOG_ERROR("Referencing to invalid Sampler!");
            return;
        }

        vkDestroySampler(m_handle, *sampler, nullptr);
        m_resources.samplers.destroy(sampler_id);
        sampler->m_handle = VK_NULL_HANDLE;
    }
}

}  // namespace lr::graphics
