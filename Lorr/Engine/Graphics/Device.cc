#include "Device.hh"

#include "Shader.hh"

namespace lr::Graphics {
template<typename T>
T get_properties_next(VkPhysicalDevice physical_device)
{
    T ret = T{ VKStruct<T>::type };
    VkPhysicalDeviceProperties2 props = { VKStruct<VkPhysicalDeviceProperties2>::type, &ret };
    vkGetPhysicalDeviceProperties2(physical_device, &props);

    return ret;
}

VkPhysicalDeviceProperties2 get_properties(VkPhysicalDevice physical_device)
{
    VkPhysicalDeviceProperties2 props = { VKStruct<VkPhysicalDeviceProperties2>::type };
    vkGetPhysicalDeviceProperties2(physical_device, &props);
    return props;
}

void Device::init(VkDevice handle, VkPhysicalDevice physical_device, VkInstance instance, eastl::span<u32> queue_indexes)
{
    m_handle = handle;
    m_physical_device = physical_device;
    m_instance = instance;
    m_tracy_ctx = TracyVkContextHostCalibrated(physical_device, m_handle);
    assert(queue_indexes.size() == m_queue_indexes.size());

    for (u32 i = 0; i < m_queues.max_size(); i++) {
        m_queue_indexes[i] = queue_indexes[i];
        create_command_queue(&m_queues[i], static_cast<CommandType>(i));
    }
}

APIResult Device::create_command_queue(CommandQueue *queue, CommandType type)
{
    ZoneScoped;

    if (!validate_handle(queue))
        return APIResult::HanldeNotInitialized;

    vkGetDeviceQueue(m_handle, get_queue_index(type), 0, &queue->m_handle);

    return APIResult::Success;
}

CommandQueue *Device::get_queue(CommandType type)
{
    return &m_queues[static_cast<usize>(type)];
}

u32 Device::get_queue_index(CommandType type)
{
    return m_queue_indexes[static_cast<usize>(type)];
}

APIResult Device::create_command_allocator(CommandAllocator *allocator, CommandType type, CommandAllocatorFlag flags)
{
    ZoneScoped;

    if (!validate_handle(allocator))
        return APIResult::HanldeNotInitialized;

    VkCommandPoolCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = static_cast<VkCommandPoolCreateFlags>(flags),
        .queueFamilyIndex = get_queue_index(type),
    };
    return CHECK(vkCreateCommandPool(m_handle, &create_info, nullptr, &allocator->m_handle));
}

void Device::delete_command_allocator(CommandAllocator *allocator)
{
    ZoneScoped;

    vkDestroyCommandPool(m_handle, allocator->m_handle, nullptr);
    allocator->m_handle = nullptr;
}

APIResult Device::create_command_list(CommandList *list, CommandAllocator *command_allocator)
{
    ZoneScoped;

    if (!validate_handle(list))
        return APIResult::HanldeNotInitialized;

    VkCommandBufferAllocateInfo allocate_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = command_allocator->m_handle,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };

    return CHECK(vkAllocateCommandBuffers(m_handle, &allocate_info, &list->m_handle));
}

void Device::delete_command_list(CommandList *list, CommandAllocator *command_allocator)
{
    ZoneScoped;

    vkFreeCommandBuffers(m_handle, command_allocator->m_handle, 1, &list->m_handle);
    list->m_handle = VK_NULL_HANDLE;
}

void Device::begin_command_list(CommandList *list)
{
    ZoneScoped;

    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    vkBeginCommandBuffer(list->m_handle, &begin_info);
}

void Device::end_command_list(CommandList *list)
{
    ZoneScoped;

    TracyVkCollect(m_tracy_ctx, list->m_handle);
    vkEndCommandBuffer(list->m_handle);
}

void Device::reset_command_allocator(CommandAllocator *allocator)
{
    ZoneScoped;

    vkResetCommandPool(m_handle, allocator->m_handle, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT);
}

void Device::submit(CommandQueue *queue, SubmitDesc *desc)
{
    ZoneScoped;

    VkSubmitInfo2 submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .waitSemaphoreInfoCount = static_cast<u32>(desc->m_wait_semas.size()),
        .pWaitSemaphoreInfos = desc->m_wait_semas.data(),
        .commandBufferInfoCount = static_cast<u32>(desc->m_lists.size()),
        .pCommandBufferInfos = desc->m_lists.data(),
        .signalSemaphoreInfoCount = static_cast<u32>(desc->m_signal_semas.size()),
        .pSignalSemaphoreInfos = desc->m_signal_semas.begin(),
    };

    vkQueueSubmit2(queue->m_handle, 1, &submit_info, nullptr);
}

APIResult Device::create_binary_semaphore(Semaphore *semaphore)
{
    ZoneScoped;

    if (!validate_handle(semaphore))
        return APIResult::HanldeNotInitialized;

    VkSemaphoreTypeCreateInfo semaphore_type_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
        .semaphoreType = VK_SEMAPHORE_TYPE_BINARY,
        .initialValue = 0,
    };

    VkSemaphoreCreateInfo semaphore_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = &semaphore_type_info,
    };
    return CHECK(vkCreateSemaphore(m_handle, &semaphore_info, nullptr, &semaphore->m_handle));
}

APIResult Device::create_timeline_semaphore(Semaphore *semaphore, u64 initial_value)
{
    ZoneScoped;

    if (!validate_handle(semaphore))
        return APIResult::HanldeNotInitialized;

    semaphore->m_value = initial_value;
    VkSemaphoreTypeCreateInfo semaphore_type_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
        .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
        .initialValue = semaphore->m_value,
    };

    VkSemaphoreCreateInfo semaphore_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = &semaphore_type_info,
    };
    return CHECK(vkCreateSemaphore(m_handle, &semaphore_info, nullptr, &semaphore->m_handle));
}

void Device::delete_semaphore(Semaphore *semaphore)
{
    ZoneScoped;

    vkDestroySemaphore(m_handle, semaphore->m_handle, nullptr);
    semaphore->m_handle = VK_NULL_HANDLE;
}

APIResult Device::wait_for_semaphore(Semaphore *semaphore, u64 desired_value, u64 timeout)
{
    ZoneScoped;

    VkSemaphoreWaitInfo wait_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
        .semaphoreCount = 1,
        .pSemaphores = &semaphore->m_handle,
        .pValues = &desired_value,
    };
    return CHECK(vkWaitSemaphores(m_handle, &wait_info, timeout));
}

APIResult Device::create_swap_chain(SwapChain *swap_chain, SwapChainDesc *desc)
{
    ZoneScoped;

    VkSurfaceKHR surface_handle = nullptr;
    VkSwapchainKHR swap_chain_handle = nullptr;

    VkWin32SurfaceCreateInfoKHR surface_info = {
        .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
        .pNext = nullptr,
        .hinstance = static_cast<HINSTANCE>(desc->m_window_instance),
        .hwnd = static_cast<HWND>(desc->m_window_handle),
    };
    APIResult result = CHECK(vkCreateWin32SurfaceKHR(m_instance, &surface_info, nullptr, &surface_handle));
    if (result != APIResult::Success) {
        LOG_ERROR("Failed to create Win32 Surface.");
        return result;
    }

    VkSurfaceCapabilitiesKHR surface_capabilities = {};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physical_device, surface_handle, &surface_capabilities);

    u32 surface_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_physical_device, surface_handle, &surface_count, nullptr);
    eastl::vector<VkSurfaceFormatKHR> surface_formats(surface_count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_physical_device, surface_handle, &surface_count, surface_formats.data());

    u32 presents_count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(m_physical_device, surface_handle, &presents_count, nullptr);
    eastl::vector<VkPresentModeKHR> present_modes(presents_count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(m_physical_device, surface_handle, &presents_count, present_modes.data());

    VkSwapchainCreateInfoKHR swap_chain_info = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .pNext = nullptr,
        .surface = surface_handle,
        .minImageCount = desc->m_frame_count,
        .imageFormat = static_cast<VkFormat>(desc->m_format),
        .imageColorSpace = desc->m_color_space,
        .imageExtent = { desc->m_width, desc->m_height },
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,
        .preTransform = surface_capabilities.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = static_cast<VkPresentModeKHR>(desc->m_present_mode),
        .clipped = VK_TRUE,
    };

    wait_for_work();
    result = CHECK(vkCreateSwapchainKHR(m_handle, &swap_chain_info, nullptr, &swap_chain_handle));
    if (result != APIResult::Success) {
        LOG_ERROR("Failed to create Win32 Swap Chain.");
        return result;
    }

    u32 semaphore_count = desc->m_frame_count + 1;
    eastl::vector<ImageID> images(desc->m_frame_count);
    eastl::vector<ImageViewID> image_views(desc->m_frame_count);
    eastl::vector<Semaphore> acquire_semas(semaphore_count);
    eastl::vector<Semaphore> present_semas(semaphore_count);

    eastl::vector<VkImage> raw_images(desc->m_frame_count);
    result = CHECK(vkGetSwapchainImagesKHR(m_handle, swap_chain_handle, &desc->m_frame_count, raw_images.data()));
    if (result != APIResult::Success)
        return {};

    for (u32 i = 0; i < desc->m_frame_count; i++) {
        auto [image_id, image] = m_resources.m_images.add_resource();

        image->m_swap_chain_image = true;
        image->init(raw_images[i], desc->m_format, desc->m_width, desc->m_height, 1, 1);

        ImageViewDesc view_desc = { .m_image = image };
        ImageViewID image_view_id = create_image_view(&view_desc);

        set_object_name(image, eastl::format("SwapChain Image {}", i));
        set_object_name(get_image_view(image_view_id), eastl::format("SwapChain ImageView {}", i));
        images[i] = image_id;
        image_views[i] = image_view_id;
    }

    for (u32 i = 0; i < semaphore_count; i++) {
        create_binary_semaphore(&acquire_semas[i]);
        create_binary_semaphore(&present_semas[i]);
    }

    swap_chain->init(
        swap_chain_handle,
        desc->m_width,
        desc->m_height,
        desc->m_frame_count,
        desc->m_format,
        eastl::move(images),
        eastl::move(image_views),
        eastl::move(acquire_semas),
        eastl::move(present_semas),
        surface_handle);

    return APIResult::Success;
}

void Device::delete_swap_chain(SwapChain *swap_chain)
{
    ZoneScoped;

    wait_for_work();
    vkDestroySwapchainKHR(m_handle, swap_chain->m_handle, nullptr);
    swap_chain->m_handle = VK_NULL_HANDLE;

    for (u32 i = 0; i < swap_chain->frame_count(); i++) {
        delete_image(swap_chain->m_images[i]);
        delete_image_view(swap_chain->m_image_views[i]);
    }

    for (u32 i = 0; i < swap_chain->semaphore_count(); i++) {
        delete_semaphore(&swap_chain->m_acquire_semas[i]);
        delete_semaphore(&swap_chain->m_present_semas[i]);
    }
}

void Device::wait_for_work()
{
    ZoneScoped;

    vkDeviceWaitIdle(m_handle);
}

APIResult Device::acquire_next_image(SwapChain *swap_chain, u32 &image_id)
{
    ZoneScoped;

    Semaphore &acquire_sema = swap_chain->m_acquire_semas[swap_chain->m_current_semaphore_id];
    auto result = CHECK(vkAcquireNextImageKHR(m_handle, swap_chain->m_handle, UINT64_MAX, acquire_sema.m_handle, nullptr, &image_id));
    swap_chain->m_current_frame_id = image_id;

    return result;
}

APIResult Device::present(SwapChain *swap_chain, CommandQueue *queue)
{
    ZoneScoped;

    Semaphore &present_sema = swap_chain->m_present_semas[swap_chain->m_current_semaphore_id];

    VkPresentInfoKHR present_info = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &present_sema.m_handle,
        .swapchainCount = 1,
        .pSwapchains = &swap_chain->m_handle,
        .pImageIndices = &swap_chain->m_current_frame_id,
    };
    APIResult result = CHECK(vkQueuePresentKHR(queue->m_handle, &present_info));
    if (result != APIResult::Success)
        return result;

    swap_chain->m_current_semaphore_id = swap_chain->next_semaphore();

    return APIResult::Success;
}

APIResult Device::create_pipeline_layout(
    PipelineLayout *layout, eastl::span<DescriptorSetLayout> layouts, eastl::span<PushConstantDesc> push_constants)
{
    ZoneScoped;

    eastl::vector<VkDescriptorSetLayout> layout_handles(layouts.size());
    for (u32 i = 0; i < layout_handles.size(); i++)
        layout_handles[i] = layouts[i].m_handle;

    VkPipelineLayoutCreateInfo pipeline_layout_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .setLayoutCount = static_cast<u32>(layout_handles.size()),
        .pSetLayouts = layout_handles.data(),
        .pushConstantRangeCount = static_cast<u32>(push_constants.size()),
        .pPushConstantRanges = push_constants.data(),
    };

    return CHECK(vkCreatePipelineLayout(m_handle, &pipeline_layout_create_info, nullptr, &layout->m_handle));
}

APIResult Device::create_graphics_pipeline(Pipeline *pipeline, GraphicsPipelineInfo *pipeline_info, PipelineAttachmentInfo *pipeline_attachment_info)
{
    ZoneScoped;

    if (!validate_handle(pipeline))
        return APIResult::HanldeNotInitialized;

    /// INPUT LAYOUT  ------------------------------------------------------------

    VkPipelineVertexInputStateCreateInfo input_layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pNext = nullptr,
        .vertexBindingDescriptionCount = static_cast<u32>(pipeline_info->m_vertex_binding_infos.size()),
        .pVertexBindingDescriptions = pipeline_info->m_vertex_binding_infos.data(),
        .vertexAttributeDescriptionCount = static_cast<u32>(pipeline_info->m_vertex_attrib_infos.size()),
        .pVertexAttributeDescriptions = pipeline_info->m_vertex_attrib_infos.data(),
    };

    /// INPUT ASSEMBLY -----------------------------------------------------------

    VkPipelineInputAssemblyStateCreateInfo input_assembly_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .pNext = nullptr,
        .topology = static_cast<VkPrimitiveTopology>(pipeline_info->m_primitive_type),
    };

    /// TESSELLATION -------------------------------------------------------------

    VkPipelineTessellationStateCreateInfo tessellation_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,
        .pNext = nullptr,
        .patchControlPoints = 0,  // TODO
    };

    /// RASTERIZER ---------------------------------------------------------------

    VkPipelineRasterizationStateCreateInfo rasterizer_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .pNext = nullptr,
        .depthClampEnable = pipeline_info->m_enable_depth_clamp,
        .polygonMode = pipeline_info->m_wireframe ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL,
        .cullMode = static_cast<VkCullModeFlags>(pipeline_info->m_cull_mode),
        .frontFace = static_cast<VkFrontFace>(!pipeline_info->m_front_face_ccw),
        .depthBiasEnable = pipeline_info->m_enable_depth_bias,
        .depthBiasConstantFactor = pipeline_info->m_depth_bias_factor,
        .depthBiasClamp = pipeline_info->m_depth_bias_clamp,
        .depthBiasSlopeFactor = pipeline_info->m_depth_slope_factor,
        .lineWidth = pipeline_info->m_line_width,
    };

    /// MULTISAMPLE --------------------------------------------------------------

    VkPipelineMultisampleStateCreateInfo multisample_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .pNext = nullptr,
        .rasterizationSamples = static_cast<VkSampleCountFlagBits>(1 << (pipeline_info->m_multisample_bit_count - 1)),
        .alphaToCoverageEnable = pipeline_info->m_enable_alpha_to_coverage,
        .alphaToOneEnable = pipeline_info->m_enable_alpha_to_one,
    };

    /// DEPTH STENCIL ------------------------------------------------------------

    VkPipelineDepthStencilStateCreateInfo depth_stencil_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .pNext = nullptr,
        .depthTestEnable = pipeline_info->m_enable_depth_test,
        .depthWriteEnable = pipeline_info->m_enable_depth_write,
        .depthCompareOp = static_cast<VkCompareOp>(pipeline_info->m_depth_compare_op),
        .depthBoundsTestEnable = pipeline_info->m_enable_depth_bounds_test,
        .stencilTestEnable = pipeline_info->m_enable_stencil_test,

        .front.compareOp = static_cast<VkCompareOp>(pipeline_info->m_stencil_front_face_op.m_compare_func),
        .front.depthFailOp = static_cast<VkStencilOp>(pipeline_info->m_stencil_front_face_op.m_depth_fail),
        .front.failOp = static_cast<VkStencilOp>(pipeline_info->m_stencil_front_face_op.m_fail),
        .front.passOp = static_cast<VkStencilOp>(pipeline_info->m_stencil_front_face_op.m_pass),

        .back.compareOp = static_cast<VkCompareOp>(pipeline_info->m_stencil_back_face_op.m_compare_func),
        .back.depthFailOp = static_cast<VkStencilOp>(pipeline_info->m_stencil_back_face_op.m_depth_fail),
        .back.failOp = static_cast<VkStencilOp>(pipeline_info->m_stencil_back_face_op.m_fail),
        .back.passOp = static_cast<VkStencilOp>(pipeline_info->m_stencil_back_face_op.m_pass),
    };

    /// COLOR BLEND --------------------------------------------------------------

    VkPipelineColorBlendStateCreateInfo color_blend_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .pNext = nullptr,
        .logicOpEnable = false,
        .attachmentCount = (u32)pipeline_info->m_blend_attachments.size(),
        .pAttachments = pipeline_info->m_blend_attachments.data(),
    };

    /// DYNAMIC STATE ------------------------------------------------------------

    VkPipelineDynamicStateCreateInfo dynamic_state_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .pNext = nullptr,
        .dynamicStateCount = static_cast<u32>(pipeline_info->m_dynamic_states.size()),
        .pDynamicStates = reinterpret_cast<VkDynamicState *>(pipeline_info->m_dynamic_states.data()),
    };

    /// GRAPHICS PIPELINE --------------------------------------------------------

    VkPipelineCreateFlags pipeline_create_flags = 0;
    if (is_feature_supported(DeviceFeature::DescriptorBuffer))
        pipeline_create_flags |= VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

    PipelineViewportStateInfo viewport_state_info(pipeline_info->m_viewports, pipeline_info->m_scissors);
    VkGraphicsPipelineCreateInfo pipeline_create_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = pipeline_attachment_info,
        .flags = pipeline_create_flags,
        .stageCount = static_cast<u32>(pipeline_info->m_shader_stages.size()),
        .pStages = pipeline_info->m_shader_stages.data(),
        .pVertexInputState = &input_layout_info,
        .pInputAssemblyState = &input_assembly_info,
        .pTessellationState = &tessellation_info,
        .pViewportState = &viewport_state_info,
        .pRasterizationState = &rasterizer_info,
        .pMultisampleState = &multisample_info,
        .pDepthStencilState = &depth_stencil_info,
        .pColorBlendState = &color_blend_info,
        .pDynamicState = &dynamic_state_info,
        .layout = pipeline_info->m_layout->m_handle,
    };

    pipeline->m_bind_point = PipelineBindPoint::Graphics;
    return CHECK(vkCreateGraphicsPipelines(m_handle, nullptr, 1, &pipeline_create_info, nullptr, &pipeline->m_handle));
}

APIResult Device::create_compute_pipeline(Pipeline *pipeline, ComputePipelineInfo *pipeline_info)
{
    ZoneScoped;

    if (!validate_handle(pipeline))
        return APIResult::HanldeNotInitialized;

    VkComputePipelineCreateInfo pipeline_create_info = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .pNext = nullptr,
        .stage = pipeline_info->m_shader_stage,
        .layout = pipeline_info->m_layout->m_handle,
    };

    pipeline->m_bind_point = PipelineBindPoint::Compute;
    return CHECK(vkCreateComputePipelines(m_handle, nullptr, 1, &pipeline_create_info, nullptr, &pipeline->m_handle));
}

void Device::delete_pipeline(Pipeline *pipeline)
{
    ZoneScoped;

    vkDestroyPipeline(m_handle, pipeline->m_handle, nullptr);
    pipeline->m_handle = VK_NULL_HANDLE;
}

APIResult Device::create_shader(Shader *shader, ShaderStage stage, eastl::span<u32> ir)
{
    ZoneScoped;

    if (!validate_handle(shader))
        return APIResult::HanldeNotInitialized;

    VkShaderModuleCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = ir.size_bytes(),
        .pCode = ir.data(),
    };

    shader->m_type = stage;
    return CHECK(vkCreateShaderModule(m_handle, &create_info, nullptr, &shader->m_handle));
}

void Device::delete_shader(Shader *shader)
{
    ZoneScoped;

    vkDestroyShaderModule(m_handle, shader->m_handle, nullptr);
    shader->m_handle = VK_NULL_HANDLE;
}

APIResult Device::create_descriptor_set_layout(
    DescriptorSetLayout *layout, eastl::span<DescriptorLayoutElement> elements, DescriptorSetLayoutFlag flags)
{
    ZoneScoped;

    if (!validate_handle(layout))
        return APIResult::HanldeNotInitialized;

    eastl::vector<VkDescriptorSetLayoutBinding> bindings(elements.size());
    eastl::vector<VkDescriptorBindingFlags> binding_flags(elements.size());
    for (u32 i = 0; i < elements.size(); i++) {
        auto &element = elements[i];
        layout->m_max_descriptor_elements += element.m_binding_info.descriptorCount;
        bindings[i] = element.m_binding_info;
        binding_flags[i] = static_cast<VkDescriptorBindingFlags>(element.m_binding_flag);
    }

    VkDescriptorSetLayoutBindingFlagsCreateInfo binding_flags_create_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
        .pNext = nullptr,
        .bindingCount = static_cast<u32>(binding_flags.size()),
        .pBindingFlags = binding_flags.data(),
    };

    VkDescriptorSetLayoutCreateFlags create_flags = 0;
    if (flags & DescriptorSetLayoutFlag::DescriptorBuffer)
        create_flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
    if (flags & DescriptorSetLayoutFlag::EmbeddedSamplers)
        create_flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_EMBEDDED_IMMUTABLE_SAMPLERS_BIT_EXT;

    VkDescriptorSetLayoutCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = &binding_flags_create_info,
        .flags = create_flags,
        .bindingCount = static_cast<u32>(bindings.size()),
        .pBindings = bindings.data(),
    };

    return CHECK(vkCreateDescriptorSetLayout(m_handle, &create_info, nullptr, &layout->m_handle));
}

void Device::delete_descriptor_set_layout(DescriptorSetLayout *layout)
{
    ZoneScoped;

    validate_handle(layout);

    vkDestroyDescriptorSetLayout(m_handle, layout->m_handle, nullptr);
    layout->m_handle = VK_NULL_HANDLE;
}

u64 Device::get_descriptor_buffer_alignment()
{
    ZoneScoped;

    auto info = get_properties_next<VkPhysicalDeviceDescriptorBufferPropertiesEXT>(m_physical_device);
    return info.descriptorBufferOffsetAlignment;
}

u64 Device::get_descriptor_size(DescriptorType type)
{
    ZoneScoped;

    auto info = get_properties_next<VkPhysicalDeviceDescriptorBufferPropertiesEXT>(m_physical_device);
    u64 sizes[] = {
        [(u32)DescriptorType::Sampler] = info.samplerDescriptorSize,
        [(u32)DescriptorType::SampledImage] = info.sampledImageDescriptorSize,
        [(u32)DescriptorType::UniformBuffer] = info.uniformBufferDescriptorSize,
        [(u32)DescriptorType::StorageImage] = info.storageImageDescriptorSize,
        [(u32)DescriptorType::StorageBuffer] = info.storageBufferDescriptorSize,
    };

    return sizes[(u32)type];
}

u64 Device::get_descriptor_set_layout_size(DescriptorSetLayout *layout)
{
    ZoneScoped;

    u64 size = 0;
    vkGetDescriptorSetLayoutSizeEXT(m_handle, layout->m_handle, &size);

    return size;
}

u64 Device::get_descriptor_set_layout_binding_offset(DescriptorSetLayout *layout, u32 binding_id)
{
    ZoneScoped;

    u64 offset = 0;
    vkGetDescriptorSetLayoutBindingOffsetEXT(m_handle, layout->m_handle, binding_id, &offset);

    return offset;
}

void Device::get_descriptor_data(const DescriptorGetInfo &info, u64 data_size, void *data_out)
{
    ZoneScoped;

    VkDescriptorAddressInfoEXT bufferInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT,
        .pNext = nullptr,
    };
    VkDescriptorImageInfo imageInfo = {};
    VkDescriptorDataEXT descriptorData = {};

    if (info.m_has_descriptor)  // Allow null descriptors
    {
        switch (info.m_type) {
            case DescriptorType::StorageBuffer:
            case DescriptorType::UniformBuffer:
                bufferInfo.address = info.m_buffer_range.address;
                bufferInfo.range = info.m_buffer_range.data_size;
                descriptorData = { .pUniformBuffer = &bufferInfo };
                break;
            case DescriptorType::StorageImage:
            case DescriptorType::SampledImage:
                imageInfo.imageView = info.m_image_view->m_handle;
                descriptorData = { .pSampledImage = &imageInfo };
                break;
            case DescriptorType::Sampler:
                imageInfo.sampler = info.m_sampler->m_handle;
                descriptorData = { .pSampledImage = &imageInfo };
                break;
            default:
                break;
        }
    }

    VkDescriptorGetInfoEXT vkInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT,
        .pNext = nullptr,
        .type = static_cast<VkDescriptorType>(info.m_type),
        .data = descriptorData,
    };

    vkGetDescriptorEXT(m_handle, &vkInfo, data_size, data_out);
}

APIResult Device::create_descriptor_pool(
    DescriptorPool *descriptor_pool, u32 max_sets, eastl::span<DescriptorPoolSize> pool_sizes, DescriptorPoolFlag flags)
{
    ZoneScoped;

    validate_handle(descriptor_pool);

    descriptor_pool->m_flags = flags;

    VkDescriptorPoolCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = static_cast<VkDescriptorPoolCreateFlags>(flags),
        .maxSets = max_sets,
        .poolSizeCount = static_cast<u32>(pool_sizes.size()),
        .pPoolSizes = pool_sizes.data(),
    };
    return CHECK(vkCreateDescriptorPool(m_handle, &create_info, nullptr, &descriptor_pool->m_handle));
}

void Device::delete_descriptor_pool(DescriptorPool *descriptor_pool)
{
    ZoneScoped;

    vkDestroyDescriptorPool(m_handle, descriptor_pool->m_handle, nullptr);
    descriptor_pool->m_handle = VK_NULL_HANDLE;
}

APIResult Device::create_descriptor_set(DescriptorSet *descriptor_set, DescriptorSetLayout *layout, DescriptorPool *descriptor_pool)
{
    ZoneScoped;

    VkDescriptorSetVariableDescriptorCountAllocateInfo set_count_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO,
        .pNext = nullptr,
        .descriptorSetCount = 1,
        .pDescriptorCounts = &layout->m_max_descriptor_elements,
    };

    VkDescriptorSetAllocateInfo allocate_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = &set_count_info,
        .descriptorPool = descriptor_pool->m_handle,
        .descriptorSetCount = 1,
        .pSetLayouts = &layout->m_handle,
    };
    return CHECK(vkAllocateDescriptorSets(m_handle, &allocate_info, &descriptor_set->m_handle));
}

void Device::delete_descriptor_set(DescriptorSet *descriptor_set, DescriptorPool *descriptor_pool)
{
    ZoneScoped;

    if (descriptor_pool->m_flags & DescriptorPoolFlag::FreeDescriptorSet)
        vkFreeDescriptorSets(m_handle, descriptor_pool->m_handle, 1, &descriptor_set->m_handle);

    descriptor_set->m_handle = VK_NULL_HANDLE;
}

void Device::update_descriptor_set(eastl::span<WriteDescriptorSet> writes, eastl::span<CopyDescriptorSet> copies)
{
    ZoneScoped;

    vkUpdateDescriptorSets(m_handle, writes.size(), writes.data(), copies.size(), copies.data());
}

eastl::tuple<u64, u64> Device::get_buffer_memory_size(BufferID buffer_id)
{
    ZoneScoped;

    return get_buffer_memory_size(get_buffer(buffer_id));
}

eastl::tuple<u64, u64> Device::get_image_memory_size(ImageID image_id)
{
    ZoneScoped;

    return get_image_memory_size(get_image(image_id));
}

eastl::tuple<u64, u64> Device::get_buffer_memory_size(Buffer *buffer)
{
    ZoneScoped;

    VkMemoryRequirements memory_requirements = {};
    vkGetBufferMemoryRequirements(m_handle, *buffer, &memory_requirements);

    return { memory_requirements.size, memory_requirements.alignment };
}

eastl::tuple<u64, u64> Device::get_image_memory_size(Image *image)
{
    ZoneScoped;

    VkMemoryRequirements memory_requirements = {};
    vkGetImageMemoryRequirements(m_handle, image->m_handle, &memory_requirements);

    return { memory_requirements.size, memory_requirements.alignment };
}

APIResult Device::create_device_memory(DeviceMemory *device_memory, DeviceMemoryDesc *desc)
{
    ZoneScoped;

    if (!validate_handle(device_memory))
        return APIResult::HanldeNotInitialized;

    u32 memory_type_index = get_memory_type_index(desc->m_flags);
    u64 available_space = ~0;

    if (is_feature_supported(DeviceFeature::MemoryBudget))
        available_space = get_heap_budget(desc->m_flags);

    if (desc->m_size > available_space) {
        LOG_ERROR("Trying to allocate more than available gpu memory budget.");
        return desc->m_flags & MemoryFlag::Device ? APIResult::OutOfDeviceMem : APIResult::OutOfHostMem;
    }

    device_memory->m_flags = desc->m_flags;
    VkMemoryAllocateFlagsInfo allocate_flags_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
        .pNext = nullptr,
        .flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT,
        .deviceMask = 0,
    };

    VkMemoryAllocateInfo allocate_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = &allocate_flags_info,
        .allocationSize = desc->m_size,
        .memoryTypeIndex = memory_type_index,
    };

    auto result = CHECK(vkAllocateMemory(m_handle, &allocate_info, nullptr, &device_memory->m_handle));
    if (result != APIResult::Success)
        return result;

    if (desc->m_flags & MemoryFlag::HostVisible)
        vkMapMemory(m_handle, device_memory->m_handle, 0, VK_WHOLE_SIZE, 0, &device_memory->m_mapped_memory);

    return APIResult::Success;
}

void Device::delete_device_memory(DeviceMemory *device_memory)
{
    ZoneScoped;

    if (device_memory->m_flags & MemoryFlag::HostVisible)
        vkUnmapMemory(m_handle, device_memory->m_handle);

    vkFreeMemory(m_handle, device_memory->m_handle, nullptr);
}

u32 Device::get_memory_type_index(MemoryFlag flags)
{
    ZoneScoped;

    VkPhysicalDeviceMemoryProperties2 device_memory_properties = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2 };
    vkGetPhysicalDeviceMemoryProperties2(m_physical_device, &device_memory_properties);

    auto vk_flags = (VkMemoryPropertyFlags)flags;
    auto &mem_props = device_memory_properties.memoryProperties;
    for (u32 i = 0; i < mem_props.memoryTypeCount; i++)
        if ((mem_props.memoryTypes[i].propertyFlags & vk_flags) == vk_flags)
            return i;

    LOG_ERROR("Memory type index is not found.");

    return -1;
}

u32 Device::get_heap_index(MemoryFlag flags)
{
    ZoneScoped;

    VkPhysicalDeviceMemoryProperties2 device_memory_properties = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2 };
    vkGetPhysicalDeviceMemoryProperties2(m_physical_device, &device_memory_properties);

    u32 i = get_memory_type_index(flags);
    return device_memory_properties.memoryProperties.memoryTypes[i].heapIndex;
}
u64 Device::get_heap_budget(MemoryFlag flags)
{
    ZoneScoped;

    if (!is_feature_supported(DeviceFeature::MemoryBudget))
        return ~0;

    VkPhysicalDeviceMemoryBudgetPropertiesEXT memory_budget = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT };
    VkPhysicalDeviceMemoryProperties2 device_memory_properties = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2, &memory_budget };
    vkGetPhysicalDeviceMemoryProperties2(m_physical_device, &device_memory_properties);

    return memory_budget.heapUsage[get_heap_index(flags)];
}

void Device::bind_memory(DeviceMemory *device_memory, BufferID buffer_id, u64 memory_offset)
{
    ZoneScoped;

    bind_memory_ex(device_memory, get_buffer(buffer_id), memory_offset);
}

void Device::bind_memory(DeviceMemory *device_memory, ImageID image_id, u64 memory_offset)
{
    ZoneScoped;

    vkBindImageMemory(m_handle, get_image(image_id)->m_handle, device_memory->m_handle, memory_offset);
}

void Device::bind_memory_ex(DeviceMemory *device_memory, Buffer *buffer, u64 memory_offset)
{
    ZoneScoped;

    vkBindBufferMemory(m_handle, *buffer, device_memory->m_handle, memory_offset);

    VkBufferDeviceAddressInfo device_address_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .pNext = nullptr,
        .buffer = *buffer,
    };
    buffer->m_device_address = vkGetBufferDeviceAddress(m_handle, &device_address_info);
}

u64 Device::get_buffer_alignment(BufferUsage usage, u64 size)
{
    constexpr static BufferUsage k_descriptor_usage = BufferUsage::ResourceDescriptor | BufferUsage::SamplerDescriptor;
    u64 alignment = 1;
    if (usage & k_descriptor_usage)
        alignment = get_descriptor_buffer_alignment();

    return alignment;
}

BufferID Device::create_buffer(BufferDesc *desc)
{
    ZoneScoped;

    u64 usage_alignment = get_buffer_alignment(desc->m_usage_flags, desc->m_data_size);
    u64 aligned_size = Memory::align_up(desc->m_data_size, usage_alignment);

    BufferUsage buffer_usage = desc->m_usage_flags | BufferUsage::BufferDeviceAddress;
    VkBufferCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,
        .size = aligned_size,
        .usage = static_cast<VkBufferUsageFlags>(buffer_usage),
    };
    VkBuffer buffer_handle = nullptr;
    auto result = CHECK(vkCreateBuffer(m_handle, &create_info, nullptr, &buffer_handle));
    if (result != APIResult::Success)
        return BufferID::Invalid;

    auto [buffer_id, buffer] = m_resources.m_buffers.add_resource();
    buffer->init(buffer_handle, aligned_size, ~0);

    return buffer_id;
}

void Device::delete_buffer(BufferID buffer_id)
{
    ZoneScoped;

    Buffer *buffer = get_buffer(buffer_id);
    m_resources.m_buffers.remove_resource(buffer_id);
    if (buffer_id == BufferID::Invalid && buffer == nullptr)
        return;

    vkDestroyBuffer(m_handle, *buffer, nullptr);
    buffer->m_handle = VK_NULL_HANDLE;
}

Buffer *Device::get_buffer(BufferID buffer_id)
{
    ZoneScoped;

    return &m_resources.m_buffers.get_resource(buffer_id);
}

APIResult Device::create_buffer_ex(Buffer &buffer, BufferDesc *desc)
{
    ZoneScoped;

    u64 usage_alignment = get_buffer_alignment(desc->m_usage_flags, desc->m_data_size);
    u64 aligned_size = Memory::align_up(desc->m_data_size, usage_alignment);

    BufferUsage buffer_usage = desc->m_usage_flags | BufferUsage::BufferDeviceAddress;
    VkBufferCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,
        .size = aligned_size,
        .usage = static_cast<VkBufferUsageFlags>(buffer_usage),
    };
    VkBuffer buffer_handle = nullptr;
    auto result = CHECK(vkCreateBuffer(m_handle, &create_info, nullptr, &buffer_handle));
    if (result != APIResult::Success)
        return result;

    VkBufferDeviceAddressInfo device_address_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .pNext = nullptr,
        .buffer = buffer_handle,
    };
    u64 device_address = vkGetBufferDeviceAddress(m_handle, &device_address_info);
    buffer.init(buffer_handle, aligned_size, device_address);

    return APIResult::Success;
}

void Device::delete_buffer_ex(Buffer &buffer)
{
    ZoneScoped;

    vkDestroyBuffer(m_handle, buffer, nullptr);
    buffer.m_handle = nullptr;
}

ImageID Device::create_image(ImageDesc *desc)
{
    ZoneScoped;

    VkImageCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = nullptr,
        .imageType = static_cast<VkImageType>(desc->m_type),
        .format = static_cast<VkFormat>(desc->m_format),
        .extent.width = desc->m_width,
        .extent.height = desc->m_height,
        .extent.depth = 1,
        .mipLevels = desc->m_mip_levels,
        .arrayLayers = desc->m_slice_count,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .usage = static_cast<VkImageUsageFlags>(desc->m_usage_flags),
        .sharingMode = VK_SHARING_MODE_CONCURRENT,
        .queueFamilyIndexCount = static_cast<u32>(desc->m_queue_indices.size()),
        .pQueueFamilyIndices = desc->m_queue_indices.data(),
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    VkImage image_handle = nullptr;
    auto result = CHECK(vkCreateImage(m_handle, &create_info, nullptr, &image_handle));
    if (result != APIResult::Success)
        return ImageID::Invalid;

    auto [image_id, image] = m_resources.m_images.add_resource();
    image->init(image_handle, desc->m_format, desc->m_width, desc->m_height, desc->m_slice_count, desc->m_mip_levels);
    return image_id;
}

void Device::delete_image(ImageID image_id)
{
    ZoneScoped;

    Image *image = get_image(image_id);
    m_resources.m_images.remove_resource(image_id);
    if (image_id == ImageID::Invalid && image == nullptr)
        return;

    if (!image->m_swap_chain_image)
        vkDestroyImage(m_handle, image->m_handle, nullptr);

    image->m_handle = VK_NULL_HANDLE;
}

Image *Device::get_image(ImageID image_id)
{
    return &m_resources.m_images.get_resource(image_id);
}

ImageViewID Device::create_image_view(ImageViewDesc *desc)
{
    ZoneScoped;

    VkImageViewCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = nullptr,
        .image = desc->m_image->m_handle,
        .viewType = static_cast<VkImageViewType>(desc->m_type),
        .format = static_cast<VkFormat>(desc->m_image->m_format),
        .components.r = static_cast<VkComponentSwizzle>(desc->m_swizzle_r),
        .components.g = static_cast<VkComponentSwizzle>(desc->m_swizzle_g),
        .components.b = static_cast<VkComponentSwizzle>(desc->m_swizzle_b),
        .components.a = static_cast<VkComponentSwizzle>(desc->m_swizzle_a),
        .subresourceRange.aspectMask = static_cast<VkImageAspectFlags>(desc->m_subresource_info.m_aspect_mask),
        .subresourceRange.baseMipLevel = desc->m_subresource_info.m_base_mip,
        .subresourceRange.levelCount = desc->m_subresource_info.m_mip_count,
        .subresourceRange.baseArrayLayer = desc->m_subresource_info.m_base_slice,
        .subresourceRange.layerCount = desc->m_subresource_info.m_slice_count,
    };

    VkImageView image_view_handle = nullptr;
    auto result = CHECK(vkCreateImageView(m_handle, &create_info, nullptr, &image_view_handle));
    if (result != APIResult::Success)
        return ImageViewID::Invalid;

    auto [image_view_id, image_view] = m_resources.m_image_views.add_resource();
    image_view->init(image_view_handle, desc->m_image->m_format, desc->m_type, desc->m_subresource_info);

    return image_view_id;
}

void Device::delete_image_view(ImageViewID image_view_id)
{
    ZoneScoped;

    ImageView *image_view = get_image_view(image_view_id);
    m_resources.m_image_views.remove_resource(image_view_id);
    if (image_view_id == ImageViewID::Invalid && image_view == nullptr)
        return;

    vkDestroyImageView(m_handle, image_view->m_handle, nullptr);
    image_view->m_handle = VK_NULL_HANDLE;
}

ImageView *Device::get_image_view(ImageViewID image_view_id)
{
    return &m_resources.m_image_views.get_resource(image_view_id);
}

SamplerID Device::create_sampler(SamplerDesc *desc)
{
    ZoneScoped;

    VkSamplerCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .magFilter = static_cast<VkFilter>(desc->m_min_filter),
        .minFilter = static_cast<VkFilter>(desc->m_min_filter),
        .mipmapMode = static_cast<VkSamplerMipmapMode>(desc->m_mip_filter),
        .addressModeU = static_cast<VkSamplerAddressMode>(desc->m_address_u),
        .addressModeV = static_cast<VkSamplerAddressMode>(desc->m_address_v),
        .addressModeW = static_cast<VkSamplerAddressMode>(desc->m_address_w),
        .mipLodBias = desc->m_mip_lod_bias,
        .anisotropyEnable = desc->m_use_anisotropy,
        .maxAnisotropy = desc->m_max_anisotropy,
        .compareEnable = desc->m_compare_op != CompareOp::Never,
        .compareOp = static_cast<VkCompareOp>(desc->m_compare_op),
        .minLod = desc->m_min_lod,
        .maxLod = desc->m_max_lod,
        .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK,
    };

    VkSampler sampler_handle = nullptr;
    auto result = CHECK(vkCreateSampler(m_handle, &create_info, nullptr, &sampler_handle));
    if (result != APIResult::Success)
        return SamplerID::Invalid;

    auto [sampler_id, sampler] = m_resources.m_samplers.add_resource();
    sampler->init(sampler_handle);

    return sampler_id;
}

void Device::delete_sampler(SamplerID sampler_id)
{
    ZoneScoped;

    Sampler *sampler = get_sampler(sampler_id);
    m_resources.m_samplers.remove_resource(sampler_id);
    if (sampler_id == SamplerID::Invalid && sampler == nullptr)
        return;

    vkDestroySampler(m_handle, sampler->m_handle, nullptr);
    sampler->m_handle = VK_NULL_HANDLE;
}

Sampler *Device::get_sampler(SamplerID sampler_id)
{
    return &m_resources.m_samplers.get_resource(sampler_id);
}

}  // namespace lr::Graphics
