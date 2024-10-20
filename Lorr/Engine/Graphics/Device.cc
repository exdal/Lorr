#include "Device.hh"

#include "Resource.hh"
#include "Shader.hh"

#include "Engine/Memory/Stack.hh"

namespace lr {
VKResult Device::init(this Device &self, const DeviceInfo &info) {
    ZoneScoped;

    self.create_command_queue(self.queues[0], CommandType::Graphics, "Graphics Command Queue");
    self.create_command_queue(self.queues[1], CommandType::Compute, "Compute Command Queue");
    self.create_command_queue(self.queues[2], CommandType::Transfer, "Transfer Command Queue");

    self.create_timeline_semaphores(self.frame_sema, 0);

    /// ALLOCATOR INITIALIZATION ///
    {
        VmaVulkanFunctions vulkan_functions = {};
        vulkan_functions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
        vulkan_functions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

        VmaAllocatorCreateInfo allocator_create_info = {
            .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
            .physicalDevice = self.physical_device,
            .device = self,
            .preferredLargeHeapBlockSize = 0,
            .pAllocationCallbacks = nullptr,
            .pDeviceMemoryCallbacks = nullptr,
            .pHeapSizeLimit = nullptr,
            .pVulkanFunctions = &vulkan_functions,
            .instance = *self.instance,
            .vulkanApiVersion = VK_API_VERSION_1_3,
            .pTypeExternalMemoryHandleTypes = nullptr,
        };

        auto result = CHECK(vmaCreateAllocator(&allocator_create_info, &self.allocator));
        if (!result) {
            LOG_ERROR("Failed to create VKAllocator! {}", (u32)result);
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

        add_descriptor_binding(LR_DESCRIPTOR_INDEX_SAMPLER, DescriptorType::Sampler, self.resources.samplers.max_resources());
        add_descriptor_binding(LR_DESCRIPTOR_INDEX_IMAGES, DescriptorType::SampledImage, self.resources.images.max_resources());
        add_descriptor_binding(
            LR_DESCRIPTOR_INDEX_STORAGE_IMAGES, DescriptorType::StorageImage, self.resources.image_views.max_resources());
        add_descriptor_binding(LR_DESCRIPTOR_INDEX_STORAGE_BUFFERS, DescriptorType::StorageBuffer, self.resources.buffers.max_resources());
        add_descriptor_binding(LR_DESCRIPTOR_INDEX_BDA_ARRAY, DescriptorType::StorageBuffer, 1);

        DescriptorPoolFlag descriptor_pool_flags = DescriptorPoolFlag::UpdateAfterBind;
        if (auto result =
                self.create_descriptor_pools(self.descriptor_pool, { .sizes = pool_sizes, .max_sets = 1, .flags = descriptor_pool_flags });
            !result) {
            LOG_ERROR("Failed to init device! Descriptor pool failed '{}'.", result);
            return result;
        }
        self.set_object_name(self.descriptor_pool, "Bindless Descriptor Pool");

        DescriptorSetLayoutFlag descriptor_set_layout_flags = DescriptorSetLayoutFlag::UpdateAfterBindPool;
        if (auto result = self.create_descriptor_set_layouts(
                self.descriptor_set_layout,
                { .elements = layout_elements, .binding_flags = binding_flags, .flags = descriptor_set_layout_flags });
            !result) {
            LOG_ERROR("Failed to init device! Descriptor set layout failed '{}'", result);
            return result;
        }
        self.set_object_name(self.descriptor_set_layout, "Bindless Descriptor Set Layout");

        if (auto result =
                self.create_descriptor_sets(self.descriptor_set, { .layout = self.descriptor_set_layout, .pool = self.descriptor_pool });
            !result) {
            LOG_ERROR("Failed to init device! Descriptor set failed '{}'.", result);
            return result;
        }
        self.set_object_name(self.descriptor_set, "Bindless Descriptor Set");

        auto &pipeline_layouts = self.resources.pipeline_layouts;
        for (u32 i = 0; i < pipeline_layouts.size(); i++) {
            PushConstantRange push_constant_range = {
                .stage = ShaderStageFlag::All,
                .offset = 0,
                .size = static_cast<u32>(i * sizeof(u32)),
            };
            PipelineLayoutInfo pipeline_layout_info = {
                .layouts = { &self.descriptor_set_layout, 1 },
                .push_constants = { &push_constant_range, !!i },
            };

            self.create_pipeline_layouts(pipeline_layouts[i], pipeline_layout_info);
            self.set_object_name(pipeline_layouts[i], std::format("Pipeline Layout ({})", i));
        }
    }

    {
        self.bda_array_buffer = self.create_buffer({
            .usage_flags = BufferUsage::Storage,
            .flags = MemoryFlag::HostSeqWrite,
            .preference = MemoryPreference::Device,
            .data_size = self.resources.buffers.max_resources() * sizeof(u64),
            .debug_name = "BDA Buffer",
        });
        Buffer &bda_array_buffer = self.buffer_at(self.bda_array_buffer);
        self.bda_array_host_addr = reinterpret_cast<u64 *>(bda_array_buffer.host_data);

        BufferDescriptorInfo buffer_descriptor_info = { .buffer = bda_array_buffer, .offset = 0, .range = VK_WHOLE_SIZE };
        WriteDescriptorSet write_info = {
            .dst_descriptor_set = self.descriptor_set,
            .dst_binding = LR_DESCRIPTOR_INDEX_BDA_ARRAY,
            .dst_element = 0,
            .count = 1,
            .type = DescriptorType::StorageBuffer,
            .buffer_info = &buffer_descriptor_info,
        };
        self.update_descriptor_sets(write_info, {});
    }

    for (u32 i = 0; i < self.frame_count; i++) {
        self.staging_buffers.emplace_back(&self, ls::mib_to_bytes(64));
    }

    return VKResult::Success;
}

void Device::shutdown(this Device &self, bool) {
    ZoneScoped;

    self.end_frame();
    for (auto &v : self.queues) {
        for (auto &allocator : v.allocators) {
            vkDestroyCommandPool(self.handle, allocator, nullptr);
        }
    }

    self.delete_semaphores(self.frame_sema);
    self.delete_descriptor_set_layouts(self.descriptor_set_layout);
    self.delete_descriptor_sets(self.descriptor_set);
    self.delete_descriptor_pools(self.descriptor_pool);
    self.delete_buffers(self.bda_array_buffer);

    for (auto &v : self.resources.cached_pipelines) {
        self.delete_pipelines(v.second);
    }

    // TODO: Print results for self.resources.*
}

VKResult Device::create_timestamp_query_pools(
    this Device &self, ls::span<TimestampQueryPool> query_pools, const TimestampQueryPoolInfo &info) {
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
        VKResult result = CHECK(vkCreateQueryPool(self, &create_info, nullptr, &query_pool.handle));
        if (result != VKResult::Success) {
            LOG_ERROR("Failed to create timestamp query pool! {}", result);
            return result;
        }

        // This is literally just memset to zero, cmd version is same but happens when queue is on execute state
        vkResetQueryPool(self, query_pool, 0, info.query_count);
    }

    return VKResult::Success;
}

void Device::delete_timestamp_query_pools(this Device &self, ls::span<TimestampQueryPool> query_pools) {
    ZoneScoped;

    for (TimestampQueryPool &query_pool : query_pools) {
        vkDestroyQueryPool(self, query_pool, nullptr);
    }
}

void Device::get_timestamp_query_pool_results(
    this Device &self, TimestampQueryPool &query_pool, u32 first_query, u32 count, ls::span<u64> time_stamps) {
    ZoneScoped;

    vkGetQueryPoolResults(
        self,
        query_pool,
        first_query,
        count,
        time_stamps.size() * sizeof(u64),
        time_stamps.data(),
        2 * sizeof(u64),
        VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);
}

VKResult Device::create_command_lists(this Device &self, ls::span<CommandList> command_lists, CommandAllocator &allocator) {
    ZoneScoped;

    VkCommandBufferAllocateInfo allocate_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = allocator,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };

    for (CommandList &cmd_list : command_lists) {
        VKResult result = CHECK(vkAllocateCommandBuffers(self, &allocate_info, &cmd_list.handle));
        if (result != VKResult::Success) {
            LOG_ERROR("Failed to create Command List! {}", result);
            return result;
        }

        cmd_list.type = allocator.type;
        cmd_list.bound_allocator = &allocator;
        cmd_list.device = &self;
    }

    return VKResult::Success;
}

void Device::delete_command_lists(this Device &self, ls::span<CommandList> command_lists) {
    ZoneScoped;

    for (CommandList &cmd_list : command_lists) {
        vkFreeCommandBuffers(self, *cmd_list.bound_allocator, 1, &cmd_list.handle);
    }
}

void Device::reset_command_allocator(this Device &self, CommandAllocator &allocator) {
    ZoneScoped;

    vkResetCommandPool(self, allocator, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT);
}

VKResult Device::create_binary_semaphores(this Device &self, ls::span<Semaphore> semaphores) {
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
        VKResult result = CHECK(vkCreateSemaphore(self, &semaphore_info, nullptr, &semaphore.handle));
        if (result != VKResult::Success) {
            LOG_ERROR("Failed to create binary semaphore! {}", result);
            return result;
        }
    }

    return VKResult::Success;
}

VKResult Device::create_timeline_semaphores(this Device &self, ls::span<Semaphore> semaphores, u64 initial_value) {
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
        semaphore.counter = initial_value;
        VKResult result = CHECK(vkCreateSemaphore(self, &semaphore_info, nullptr, &semaphore.handle));
        if (result != VKResult::Success) {
            LOG_ERROR("Failed to create timeline semaphore! {}", result);
            return result;
        }
    }

    return VKResult::Success;
}

void Device::delete_semaphores(this Device &self, ls::span<Semaphore> semaphores) {
    ZoneScoped;

    for (Semaphore &semaphore : semaphores) {
        vkDestroySemaphore(self, semaphore, nullptr);
    }
}

VKResult Device::wait_for_semaphore(this Device &self, Semaphore &semaphore, u64 desired_value, u64 timeout) {
    ZoneScoped;

    VkSemaphoreWaitInfo wait_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
        .pNext = nullptr,
        .flags = 0,
        .semaphoreCount = 1,
        .pSemaphores = &semaphore.handle,
        .pValues = &desired_value,
    };
    return CHECK(vkWaitSemaphores(self, &wait_info, timeout));
}

ls::result<u64, VKResult> Device::get_semaphore_counter(this Device &self, Semaphore &semaphore) {
    ZoneScoped;

    u64 value = 0;
    auto result = CHECK(vkGetSemaphoreCounterValue(self, semaphore, &value));

    return ls::result(value, result);
}

VKResult Device::create_swap_chain(this Device &self, SwapChain &swap_chain, const SwapChainInfo &info) {
    ZoneScoped;

    self.wait_for_work();

    VkPresentModeKHR present_mode = self.frame_count == 1 ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_IMMEDIATE_KHR;

    vkb::SwapchainBuilder builder(self.handle, info.surface);
    builder.set_desired_min_image_count(self.frame_count);
    builder.set_desired_extent(info.extent.width, info.extent.height);
    builder.set_desired_format({ VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR });
    builder.set_desired_present_mode(present_mode);
    builder.set_image_usage_flags(static_cast<VkImageUsageFlags>(ImageUsage::ColorAttachment | ImageUsage::TransferDst));
    auto result = builder.build();

    if (!result) {
        LOG_ERROR("Failed to create SwapChain!");
        return static_cast<VKResult>(result.vk_result());
    }

    swap_chain = {};
    swap_chain.format = static_cast<Format>(result->image_format);
    swap_chain.extent = { .width = result->extent.width, .height = result->extent.height, .depth = 1 };

    swap_chain.acquire_semas.resize(self.frame_count);
    swap_chain.present_semas.resize(self.frame_count);
    self.create_binary_semaphores(swap_chain.acquire_semas);
    self.create_binary_semaphores(swap_chain.present_semas);
    swap_chain.handle = result.value();
    swap_chain.surface = info.surface;

    u32 i = 0;
    for (Semaphore &v : swap_chain.acquire_semas) {
        self.set_object_name(v, std::format("SwapChain Acquire Sema {}", i++));
    }

    i = 0;
    for (Semaphore &v : swap_chain.present_semas) {
        self.set_object_name(v, std::format("SwapChain Present Sema {}", i++));
    }

    self.set_object_name_raw<VK_OBJECT_TYPE_SWAPCHAIN_KHR>(swap_chain.handle.swapchain, "SwapChain");

    return VKResult::Success;
}

void Device::delete_swap_chains(this Device &self, ls::span<SwapChain> swap_chains) {
    ZoneScoped;

    self.wait_for_work();

    for (const SwapChain &swap_chain : swap_chains) {
        vkb::destroy_swapchain(swap_chain.handle);
    }
}

VKResult Device::get_swapchain_images(this Device &self, SwapChain &swap_chain, ls::span<ImageID> images) {
    ZoneScoped;
    u32 image_count = images.size();
    ls::static_vector<VkImage, Limits::FrameCount> images_raw(image_count);

    auto result = CHECK(vkGetSwapchainImagesKHR(self, swap_chain.handle, &image_count, images_raw.data()));
    if (!result) {
        LOG_ERROR("Failed to get SwapChain images! {}", result);
        return result;
    }

    for (u32 i = 0; i < images.size(); i++) {
        VkImage &image_handle = images_raw[i];
        auto image_result = self.resources.images.create(
            ImageUsage::ColorAttachment, swap_chain.format, swap_chain.extent, ImageAspect::Color, 1, 1, nullptr, image_handle);

        images[i] = image_result.id;
    }

    return VKResult::Success;
}

void Device::wait_for_work(this Device &self) {
    ZoneScoped;

    vkDeviceWaitIdle(self);
}

usize Device::new_frame(this Device &self) {
    ZoneScoped;

    i64 sema_counter = static_cast<i64>(self.frame_sema.counter);
    u64 wait_val = static_cast<u64>(std::max<i64>(0, sema_counter - static_cast<i64>(self.frame_count - 1)));

    self.wait_for_semaphore(self.frame_sema, wait_val);
    return self.sema_index();
}

ls::result<u32, VKResult> Device::acquire_next_image(this Device &self, SwapChain &swap_chain, Semaphore &acquire_sema) {
    ZoneScoped;

    u32 image_id = 0;
    auto result = static_cast<VKResult>(vkAcquireNextImageKHR(self, swap_chain.handle, UINT64_MAX, acquire_sema, nullptr, &image_id));
    if (result != VKResult::Success && result != VKResult::Suboptimal) {
        return result;
    }

    return ls::result(image_id, result);
}

void Device::end_frame(this Device &self) {
    ZoneScoped;

    auto checkndelete_fn = [&](auto &container, u64 sema_counter, const auto &deleter_fn) {
        for (auto i = container.begin(); i != container.end();) {
            auto &[v, timeline_val] = *i;
            if (sema_counter > timeline_val) {
                deleter_fn(v);
                i = container.erase(i);
            } else {
                i++;
            }
        }
    };

    for (CommandQueue &v : self.queues) {
        u64 queue_counter = self.get_semaphore_counter(v.semaphore);
        checkndelete_fn(v.garbage_samplers, queue_counter, [&self](ls::span<SamplerID> s) { self.delete_samplers(s); });
        checkndelete_fn(v.garbage_image_views, queue_counter, [&self](ls::span<ImageViewID> s) { self.delete_image_views(s); });
        checkndelete_fn(v.garbage_images, queue_counter, [&self](ls::span<ImageID> s) { self.delete_images(s); });
        checkndelete_fn(v.garbage_buffers, queue_counter, [&self](ls::span<BufferID> s) { self.delete_buffers(s); });
        checkndelete_fn(v.garbage_command_lists, queue_counter, [&self](ls::span<CommandList> s) { self.delete_command_lists(s); });
    }

    self.frame_sema.advance();
}

VKResult Device::create_pipeline_layouts(this Device &self, ls::span<PipelineLayout> pipeline_layouts, const PipelineLayoutInfo &info) {
    ZoneScoped;
    memory::ScopedStack stack;

    auto vk_pipeline_layouts = stack.alloc<VkDescriptorSetLayout>(info.layouts.size());
    for (u32 i = 0; i < vk_pipeline_layouts.size(); i++) {
        vk_pipeline_layouts[i] = info.layouts[i];
    }

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
        auto result = CHECK(vkCreatePipelineLayout(self, &pipeline_layout_create_info, nullptr, &layout_handle));
        if (result != VKResult::Success) {
            LOG_ERROR("Failed to create Pipeline Layout! {}", result);
            return result;
        }

        layout.handle = layout_handle;
    }

    return VKResult::Success;
}

void Device::delete_pipeline_layouts(this Device &self, ls::span<PipelineLayout> pipeline_layout) {
    ZoneScoped;

    for (PipelineLayout &layout : pipeline_layout) {
        vkDestroyPipelineLayout(self, layout, nullptr);
    }
}

ls::result<PipelineID, VKResult> Device::create_graphics_pipeline(this Device &self, const GraphicsPipelineInfo &info) {
    ZoneScoped;
    memory::ScopedStack stack;

    VkPipelineRenderingCreateInfo rendering_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .pNext = nullptr,
        .viewMask = 0,
        .colorAttachmentCount = static_cast<u32>(info.color_attachment_formats.size()),
        .pColorAttachmentFormats = reinterpret_cast<const VkFormat *>(info.color_attachment_formats.data()),
        .depthAttachmentFormat = static_cast<VkFormat>(info.depth_attachment_format.value_or(Format::Unknown)),
        .stencilAttachmentFormat = static_cast<VkFormat>(info.stencil_attachment_format.value_or(Format::Unknown)),
    };

    // Viewport State

    VkViewport default_viewport = { .x = 0, .y = 0, .width = 1, .height = 1, .minDepth = 0.0f, .maxDepth = 1.0f };
    VkRect2D default_scissors = { .offset = { .x = 0, .y = 0 }, .extent = { .width = 1, .height = 1 } };
    VkPipelineViewportStateCreateInfo viewport_state_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .viewportCount = 1,
        .pViewports = &default_viewport,
        .scissorCount = 1,
        .pScissors = &default_scissors,
    };

    // Shaders

    auto vk_shader_stage_infos = stack.alloc<VkPipelineShaderStageCreateInfo>(info.shader_ids.size());
    for (u32 i = 0; i < vk_shader_stage_infos.size(); i++) {
        auto &vk_info = vk_shader_stage_infos[i];
        auto &v = info.shader_ids[i];
        Shader &shader = self.shader_at(v);

        vk_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vk_info.pNext = nullptr;
        vk_info.flags = 0;
        vk_info.stage = static_cast<VkShaderStageFlagBits>(shader.stage);
        vk_info.module = shader;
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
        .topology = static_cast<VkPrimitiveTopology>(info.rasterizer_state.primitive_type),
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
        .depthClampEnable = info.rasterizer_state.enable_depth_clamp,
        .rasterizerDiscardEnable = false,
        .polygonMode = info.rasterizer_state.enable_wireframe ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL,
        .cullMode = static_cast<VkCullModeFlags>(info.rasterizer_state.cull_mode),
        .frontFace = static_cast<VkFrontFace>(!info.rasterizer_state.front_face_ccw),
        .depthBiasEnable = info.rasterizer_state.enable_depth_bias,
        .depthBiasConstantFactor = info.rasterizer_state.depth_bias_factor,
        .depthBiasClamp = info.rasterizer_state.depth_bias_clamp,
        .depthBiasSlopeFactor = info.rasterizer_state.depth_slope_factor,
        .lineWidth = info.rasterizer_state.line_width,
    };

    // Multisampling

    VkPipelineMultisampleStateCreateInfo multisample_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .rasterizationSamples = static_cast<VkSampleCountFlagBits>(1 << (info.multisample_state.multisample_bit_count - 1)),
        .sampleShadingEnable = false,
        .minSampleShading = 0,
        .pSampleMask = nullptr,
        .alphaToCoverageEnable = info.multisample_state.enable_alpha_to_coverage,
        .alphaToOneEnable = info.multisample_state.enable_alpha_to_one,
    };

    // Depth Stencil

    VkPipelineDepthStencilStateCreateInfo depth_stencil_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .depthTestEnable = info.depth_stencil_state.enable_depth_test,
        .depthWriteEnable = info.depth_stencil_state.enable_depth_write,
        .depthCompareOp = static_cast<VkCompareOp>(info.depth_stencil_state.depth_compare_op),
        .depthBoundsTestEnable = info.depth_stencil_state.enable_depth_bounds_test,
        .stencilTestEnable = info.depth_stencil_state.enable_stencil_test,
        .front = info.depth_stencil_state.stencil_front_face_op,
        .back = info.depth_stencil_state.stencil_back_face_op,
        .minDepthBounds = 0,
        .maxDepthBounds = 0,
    };

    // Color Blending

    /// DYNAMIC STATE ------------------------------------------------------------

    /// GRAPHICS PIPELINE --------------------------------------------------------

    VkPipelineCreateFlags pipeline_create_flags = 0;
    if (self.is_feature_supported(DeviceFeature::DescriptorBuffer))
        pipeline_create_flags |= VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

    VkPipeline pipeline_handle = VK_NULL_HANDLE;
    auto result = CHECK(vkCreateGraphicsPipelines(self, nullptr, 1, &pipeline_create_info, nullptr, &pipeline_handle));
    if (result != VKResult::Success) {
        LOG_ERROR("Failed to create Graphics Pipeline! {}", result);
        return result;
    }

    auto pipeline = self.resources.pipelines.create(PipelineBindPoint::Graphics, info.layout_id, pipeline_handle);
    if (!pipeline) {
        LOG_ERROR("Failed to allocate Graphics Pipeline!");
        return VKResult::OutOfPoolMem;
    }

    return pipeline.id;
};

ls::result<PipelineID, VKResult> Device::create_compute_pipeline(this Device &self, const ComputePipelineInfo &info) {
    ZoneScoped;

    Shader &shader = self.shader_at(info.shader_id);
    PipelineShaderStageInfo shader_info = {
        .shader_stage = shader.stage,
        .module = shader,
        .entry_point = "main",
    };

    PipelineLayout &pipeline_layout = self.pipeline_layout_at(info.layout_id);

    VkComputePipelineCreateInfo pipeline_create_info = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .stage = shader_info,
        .layout = pipeline_layout,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = 0,
    };

    VkPipeline pipeline_handle = VK_NULL_HANDLE;
    auto result = CHECK(vkCreateComputePipelines(self, nullptr, 1, &pipeline_create_info, nullptr, &pipeline_handle));
    if (result != VKResult::Success) {
        LOG_ERROR("Failed to create Compute Pipeline! {}", result);
        return result;
    }

    auto pipeline = self.resources.pipelines.create(PipelineBindPoint::Compute, info.layout_id, pipeline_handle);
    if (!pipeline) {
        LOG_ERROR("Failed to allocate Compute Pipeline!");
        return VKResult::OutOfPoolMem;
    }

    return pipeline.id;
}

void Device::delete_pipelines(this Device &self, ls::span<PipelineID> pipelines) {
    ZoneScoped;

    for (const PipelineID pipeline_id : pipelines) {
        Pipeline &pipeline = self.resources.pipelines.get(pipeline_id);
        if (!pipeline) {
            LOG_ERROR("Referencing to invalid Pipeline.");
            return;
        }

        vkDestroyPipeline(self, pipeline, nullptr);
        self.resources.pipelines.destroy(pipeline_id);
        pipeline.handle = VK_NULL_HANDLE;
    }
}

ls::result<ShaderID, VKResult> Device::create_shader(this Device &self, ShaderStageFlag stage, ls::span<u32> ir) {
    ZoneScoped;

    if (result != VKResult::Success) {
        LOG_ERROR("Failed to create shader! {}", result);
        return result;
    }

    auto shader = self.resources.shaders.create(stage, shader_module);
    if (!shader) {
        LOG_ERROR("Failed to allocate shader!");
        return VKResult::OutOfPoolMem;
    }

    return shader.id;
}

void Device::delete_shaders(this Device &self, ls::span<ShaderID> shaders) {
    ZoneScoped;

    for (const ShaderID shader_id : shaders) {
        Shader &shader = self.resources.shaders.get(shader_id);
        if (!shader) {
            LOG_ERROR("Referencing to invalid Shader.");
            return;
        }

        vkDestroyShaderModule(self, shader, nullptr);
        self.resources.shaders.destroy(shader_id);
        shader.handle = VK_NULL_HANDLE;
    }
}

VKResult Device::create_descriptor_pools(this Device &self, ls::span<DescriptorPool> descriptor_pools, const DescriptorPoolInfo &info) {
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
        auto result = CHECK(vkCreateDescriptorPool(self, &create_info, nullptr, &pool_handle));
        if (result != VKResult::Success) {
            LOG_ERROR("Failed to create Descriptor Pool! {}", result);
            return result;
        }

        descriptor_pool = DescriptorPool(info.flags, pool_handle);
    }

    return VKResult::Success;
}

void Device::delete_descriptor_pools(this Device &self, ls::span<DescriptorPool> descriptor_pools) {
    ZoneScoped;

    for (DescriptorPool &pool : descriptor_pools) {
        vkDestroyDescriptorPool(self, pool, nullptr);
    }
}

VKResult Device::create_descriptor_set_layouts(
    this Device &self, ls::span<DescriptorSetLayout> descriptor_set_layouts, const DescriptorSetLayoutInfo &info) {
    ZoneScoped;

    LS_EXPECT(info.elements.size() == info.binding_flags.size());
    VkDescriptorSetLayoutBindingFlagsCreateInfo binding_flags_create_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
        .pNext = nullptr,
        .bindingCount = static_cast<u32>(info.binding_flags.size()),
        .pBindingFlags = reinterpret_cast<const VkDescriptorBindingFlags *>(info.binding_flags.data()),
    };

    VkDescriptorSetLayoutCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = &binding_flags_create_info,
        .flags = static_cast<VkDescriptorSetLayoutCreateFlags>(info.flags),
        .bindingCount = static_cast<u32>(info.elements.size()),
        .pBindings = reinterpret_cast<const DescriptorSetLayoutElement::VkType *>(info.elements.data()),
    };

    for (DescriptorSetLayout &descriptor_set_layout : descriptor_set_layouts) {
        VkDescriptorSetLayout layout_handle = VK_NULL_HANDLE;
        auto result = CHECK(vkCreateDescriptorSetLayout(self, &create_info, nullptr, &layout_handle));
        if (result != VKResult::Success) {
            LOG_ERROR("Failed to create Descriptor Set Layout! {}", result);
            return result;
        }

        descriptor_set_layout = DescriptorSetLayout(layout_handle);
    }

    return VKResult::Success;
}

void Device::delete_descriptor_set_layouts(this Device &self, ls::span<DescriptorSetLayout> layouts) {
    ZoneScoped;

    for (DescriptorSetLayout &layout : layouts) {
        vkDestroyDescriptorSetLayout(self, layout, nullptr);
    }
}

VKResult Device::create_descriptor_sets(this Device &self, ls::span<DescriptorSet> descriptor_sets, const DescriptorSetInfo &info) {
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
        .pSetLayouts = &info.layout.handle,
    };

    for (DescriptorSet &descriptor_set : descriptor_sets) {
        VkDescriptorSet descriptor_set_handle = VK_NULL_HANDLE;
        auto result = CHECK(vkAllocateDescriptorSets(self, &allocate_info, &descriptor_set_handle));
        if (result != VKResult::Success) {
            LOG_ERROR("Failed to create Descriptor Set! {}", result);
            return result;
        }

        descriptor_set = DescriptorSet(&info.pool, descriptor_set_handle);
    }

    return VKResult::Success;
}

void Device::delete_descriptor_sets(this Device &self, ls::span<DescriptorSet> descriptor_sets) {
    ZoneScoped;

    for (const DescriptorSet &descriptor_set : descriptor_sets) {
        DescriptorPool *pool = descriptor_set.pool;
        if (pool->flags & DescriptorPoolFlag::FreeDescriptorSet) {
            continue;
        }

        vkFreeDescriptorSets(self, *pool, 1, &descriptor_set.handle);
    }
}

void Device::update_descriptor_sets(this Device &self, ls::span<WriteDescriptorSet> writes, ls::span<CopyDescriptorSet> copies) {
    ZoneScoped;

    vkUpdateDescriptorSets(
        self,
        writes.size(),
        reinterpret_cast<const VkWriteDescriptorSet *>(writes.data()),
        copies.size(),
        reinterpret_cast<const VkCopyDescriptorSet *>(copies.data()));
}

ls::result<BufferID, VKResult> Device::create_buffer(this Device &self, const BufferInfo &info) {
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
    auto result = CHECK(vmaCreateBuffer(self.allocator, &create_info, &allocation_info, &buffer_handle, &allocation, &allocation_result));
    if (result != VKResult::Success) {
        LOG_ERROR("Failed to create Buffer! {}", result);
        return result;
    }
#if LS_DEBUG
    vmaSetAllocationName(self.allocator, allocation, info.debug_name.data());
#endif

    VkBufferDeviceAddressInfo device_address_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .pNext = nullptr,
        .buffer = buffer_handle,
    };
    u64 device_address = vkGetBufferDeviceAddress(self, &device_address_info);

    auto buffer = self.resources.buffers.create(info.data_size, allocation_result.pMappedData, device_address, allocation, buffer_handle);
    if (!buffer) {
        LOG_ERROR("Failed to allocate Buffer!");
        return VKResult::OutOfPoolMem;
    }

    if (self.bda_array_host_addr) {
        self.bda_array_host_addr[static_cast<usize>(buffer.id)] = device_address;
    }

    if (!info.debug_name.empty()) {
        self.set_object_name(*buffer.resource, info.debug_name);
    }

    return buffer.id;
}

void Device::delete_buffers(this Device &self, ls::span<BufferID> buffers) {
    ZoneScoped;

    for (BufferID buffer_id : buffers) {
        Buffer &buffer = self.resources.buffers.get(buffer_id);
        if (!buffer) {
            LOG_ERROR("Referencing to invalid Buffer!");
            return;
        }

        vmaDestroyBuffer(self.allocator, buffer, buffer.allocation);
        self.resources.buffers.destroy(buffer_id);
        buffer.handle = VK_NULL_HANDLE;
    }
}

MemoryRequirements Device::memory_requirements(this Device &self, BufferID buffer_id) {
    ZoneScoped;

    VkMemoryRequirements vk_info = {};
    vkGetBufferMemoryRequirements(self, self.buffer_at(buffer_id), &vk_info);

    return { .size = vk_info.size, .alignment = vk_info.alignment, .memory_type_bits = vk_info.memoryTypeBits };
}

ls::result<ImageID, VKResult> Device::create_image(this Device &self, const ImageInfo &info) {
    ZoneScoped;

    LS_EXPECT(!info.extent.is_zero());

    VkSharingMode sharing_mode = VK_SHARING_MODE_EXCLUSIVE;
    if (!info.queue_indices.empty()) {
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
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    VmaAllocationCreateInfo allocation_info = {
        .flags = 0,
        .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        .requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
        .preferredFlags = 0,
        .memoryTypeBits = 0,
        .pool = nullptr,
        .pUserData = nullptr,
        .priority = 0.5f,
    };

    VkImage image_handle = nullptr;
    VmaAllocation allocation = nullptr;

    auto result = CHECK(vmaCreateImage(self.allocator, &create_info, &allocation_info, &image_handle, &allocation, nullptr));
    if (result != VKResult::Success) {
        LOG_ERROR("Failed to create Image! {}", result);
        return result;
    }

    auto image = self.resources.images.create(
        info.usage_flags, info.format, info.extent, info.image_aspect, info.slice_count, info.mip_levels, allocation, image_handle);
    if (!image) {
        LOG_ERROR("Failed to allocate Image!");
        return VKResult::OutOfPoolMem;
    }

    if (!info.debug_name.empty()) {
        self.set_object_name(*image.resource, info.debug_name);
    }

    return image.id;
}

void Device::delete_images(this Device &self, ls::span<ImageID> images) {
    ZoneScoped;

    for (const ImageID image_id : images) {
        Image &image = self.resources.images.get(image_id);
        if (!image) {
            LOG_ERROR("Referencing to invalid Image!");
            return;
        }

        if (image.allocation) {
            // if set to falst, we are most likely destroying sc images
            vmaDestroyImage(self.allocator, image, image.allocation);
        }
        self.resources.images.destroy(image_id);
        image.handle = VK_NULL_HANDLE;
    }
}

MemoryRequirements Device::memory_requirements(this Device &self, ImageID image_id) {
    ZoneScoped;

    VkMemoryRequirements vk_info = {};
    vkGetImageMemoryRequirements(self, self.image_at(image_id), &vk_info);

    return { .size = vk_info.size, .alignment = vk_info.alignment, .memory_type_bits = vk_info.memoryTypeBits };
}

ls::result<ImageViewID, VKResult> Device::create_image_view(this Device &self, const ImageViewInfo &info) {
    ZoneScoped;

    auto &image = self.image_at(info.image_id);
    VkImageViewCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .image = image,
        .viewType = static_cast<VkImageViewType>(info.type),
        .format = static_cast<VkFormat>(image.format),
        .components = { static_cast<VkComponentSwizzle>(info.swizzle_r),
                        static_cast<VkComponentSwizzle>(info.swizzle_g),
                        static_cast<VkComponentSwizzle>(info.swizzle_b),
                        static_cast<VkComponentSwizzle>(info.swizzle_a) },
        .subresourceRange = info.subresource_range,
    };

    VkImageView image_view_handle = nullptr;
    auto result = CHECK(vkCreateImageView(self, &create_info, nullptr, &image_view_handle));
    if (result != VKResult::Success) {
        LOG_ERROR("Failed to create Image View! {}", result);
        return result;
    }

    auto image_view = self.resources.image_views.create(image.format, info.type, info.subresource_range, image_view_handle);
    if (!image_view) {
        LOG_ERROR("Failed to allocate Image View!");
        return VKResult::OutOfPoolMem;
    }

    ls::static_vector<WriteDescriptorSet, 2> descriptor_writes = {};
    ImageDescriptorInfo sampled_descriptor_info = { .image_view = *image_view.resource, .image_layout = ImageLayout::ColorReadOnly };
    ImageDescriptorInfo storage_descriptor_info = { .image_view = *image_view.resource, .image_layout = ImageLayout::General };

    WriteDescriptorSet sampled_write_set_info = {
        .dst_descriptor_set = self.descriptor_set,
        .dst_binding = LR_DESCRIPTOR_INDEX_IMAGES,
        .dst_element = static_cast<u32>(image_view.id),
        .count = 1,
        .type = DescriptorType::SampledImage,
        .image_info = &sampled_descriptor_info,
    };
    WriteDescriptorSet storage_write_set_info = {
        .dst_descriptor_set = self.descriptor_set,
        .dst_binding = LR_DESCRIPTOR_INDEX_STORAGE_IMAGES,
        .dst_element = static_cast<u32>(image_view.id),
        .count = 1,
        .type = DescriptorType::StorageImage,
        .image_info = &storage_descriptor_info,
    };
    if (image.usage_flags & ImageUsage::Sampled) {
        descriptor_writes.push_back(sampled_write_set_info);
    }
    if (image.usage_flags & ImageUsage::Storage) {
        descriptor_writes.push_back(storage_write_set_info);
    }

    if (!descriptor_writes.empty()) {
        self.update_descriptor_sets(descriptor_writes, {});
    }

    if (!info.debug_name.empty()) {
        self.set_object_name(*image_view.resource, info.debug_name);
    }

    return image_view.id;
}

void Device::delete_image_views(this Device &self, ls::span<ImageViewID> image_views) {
    ZoneScoped;

    for (ImageViewID image_view_id : image_views) {
        ImageView &image_view = self.resources.image_views.get(image_view_id);
        if (!image_view) {
            LOG_ERROR("Referencing to invalid Image View!");
            return;
        }

        vkDestroyImageView(self, image_view.handle, nullptr);
        self.resources.image_views.destroy(image_view_id);
        image_view.handle = VK_NULL_HANDLE;
    }
}

ls::result<SamplerID, VKResult> Device::create_sampler(this Device &self, const SamplerInfo &info) {
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
    auto result = CHECK(vkCreateSampler(self, &create_info, nullptr, &sampler_handle));
    if (result != VKResult::Success) {
        LOG_ERROR("Failed to create Sampler! {}", result);
        return result;
    }

    auto sampler = self.resources.samplers.create(sampler_handle);
    if (!sampler) {
        LOG_ERROR("Failed to allocate Sampler!");
        return VKResult::OutOfPoolMem;
    }

    ImageDescriptorInfo descriptor_info = { .sampler = *sampler.resource };
    WriteDescriptorSet write_set_info = {
        .dst_descriptor_set = self.descriptor_set,
        .dst_binding = LR_DESCRIPTOR_INDEX_SAMPLER,
        .dst_element = static_cast<u32>(sampler.id),
        .count = 1,
        .type = DescriptorType::Sampler,
        .image_info = &descriptor_info,
    };
    self.update_descriptor_sets(write_set_info, {});

    if (!info.debug_name.empty()) {
        self.set_object_name(*sampler.resource, info.debug_name);
    }

    return sampler.id;
}

void Device::delete_samplers(this Device &self, ls::span<SamplerID> samplers) {
    ZoneScoped;

    for (SamplerID sampler_id : samplers) {
        Sampler &sampler = self.resources.samplers.get(sampler_id);
        if (!sampler) {
            LOG_ERROR("Referencing to invalid Sampler!");
            return;
        }

        vkDestroySampler(self, sampler, nullptr);
        self.resources.samplers.destroy(sampler_id);
        sampler.handle = VK_NULL_HANDLE;
    }
}

void Device::set_image_data(this Device &self, ImageID image_id, const void *data, ImageLayout image_layout) {
    ZoneScoped;

    auto &image = self.image_at(image_id);
    auto &queue = self.queue_at(CommandType::Graphics);
    auto cmd_list = queue.begin_command_list(0);

    cmd_list.image_transition(ImageBarrier{
        .src_access = PipelineAccess::None,
        .dst_access = PipelineAccess::TransferReadWrite,
        .old_layout = ImageLayout::Undefined,
        .new_layout = ImageLayout::TransferDst,
        .subresource_range = image.whole_subresource_range(),
    });

    queue.end_command_list(cmd_list);
    queue.submit(0, {});
    queue.wait_for_work();
}
VKResult Device::create_command_queue(this Device &self, CommandQueue &command_queue, ) {
    ZoneScoped;
    memory::ScopedStack stack;

    std::string_view callocator_name = stack.format("{} Command Allocator", debug_name);
    std::string_view timeline_sema_name = stack.format("{} Semaphore", debug_name);

    self.create_timeline_semaphores(command_queue.semaphore, 0);
    self.create_command_allocators(command_queue.allocators, { .type = type, .debug_name = callocator_name });
    self.set_object_name(command_queue, debug_name);
    self.set_object_name(command_queue.semaphore, timeline_sema_name);

    return VKResult::Success;
}

VKResult Device::create_command_allocators(
    this Device &self, ls::span<CommandAllocator> command_allocators, const CommandAllocatorInfo &info) {
    ZoneScoped;

    VkCommandPoolCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = static_cast<VkCommandPoolCreateFlags>(info.flags),
        .queueFamilyIndex = self.queue_at(info.type).family_index,
    };

    for (CommandAllocator &allocator : command_allocators) {
        VKResult result = CHECK(vkCreateCommandPool(self, &create_info, nullptr, &allocator.handle));
        if (result != VKResult::Success) {
            LOG_ERROR("Failed to create Command Allocator! {}", result);
            return result;
        }

        allocator.type = info.type;
        self.set_object_name(allocator, info.debug_name);
    }

    return VKResult::Success;
}

}  // namespace lr
