#include "Device.hh"

#include "Shader.hh"

namespace lr::Graphics
{
void Device::init(VkDevice handle, PhysicalDevice *physical_device)
{
    m_handle = handle;
    m_physical_device = physical_device;
    m_tracy_ctx = TracyVkContextHostCalibrated(physical_device->m_handle, m_handle);

    for (u32 i = 0; i < m_queues.max_size(); i++)
        create_command_queue(&m_queues[i], static_cast<CommandTypeMask>(1 << i));
}

APIResult Device::create_command_queue(CommandQueue *queue, CommandTypeMask type_mask)
{
    ZoneScoped;

    if (!validate_handle(queue))
        return APIResult::HanldeNotInitialized;

    vkGetDeviceQueue(m_handle, m_physical_device->get_queue_info(type_mask).m_index, 0, &queue->m_handle);

    return APIResult::Success;
}

CommandQueue *Device::get_queue(CommandType type)
{
    return &m_queues[static_cast<usize>(type)];
}

APIResult Device::create_command_allocator(CommandAllocator *allocator, CommandTypeMask type_mask, CommandAllocatorFlag flags)
{
    ZoneScoped;

    if (!validate_handle(allocator))
        return APIResult::HanldeNotInitialized;

    VkCommandPoolCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = static_cast<VkCommandPoolCreateFlags>(flags),
        .queueFamilyIndex = m_physical_device->get_queue_info(type_mask).m_index,
    };
    return static_cast<APIResult>(vkCreateCommandPool(m_handle, &create_info, nullptr, &allocator->m_handle));
}

void Device::delete_command_allocator(CommandAllocator *allocator)
{
    ZoneScoped;

    vkDestroyCommandPool(m_handle, *allocator, nullptr);
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

    return static_cast<APIResult>(vkAllocateCommandBuffers(m_handle, &allocate_info, &list->m_handle));
}

void Device::delete_command_list(CommandList *list, CommandAllocator *command_allocator)
{
    ZoneScoped;

    vkFreeCommandBuffers(m_handle, *command_allocator, 1, &list->m_handle);
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
    vkBeginCommandBuffer(static_cast<VkCommandBuffer>(*list), &begin_info);
}

void Device::end_command_list(CommandList *list)
{
    ZoneScoped;

    TracyVkCollect(m_pTracyCtx, list->m_handle);
    vkEndCommandBuffer(static_cast<VkCommandBuffer>(*list));
}

void Device::reset_command_allocator(CommandAllocator *allocator)
{
    ZoneScoped;

    vkResetCommandPool(m_handle, *allocator, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT);
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

    vkQueueSubmit2(*queue, 1, &submit_info, nullptr);
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
    return static_cast<APIResult>(vkCreateSemaphore(m_handle, &semaphore_info, nullptr, &semaphore->m_handle));
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
    return static_cast<APIResult>(vkCreateSemaphore(m_handle, &semaphore_info, nullptr, &semaphore->m_handle));
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
    return static_cast<APIResult>(vkWaitSemaphores(m_handle, &wait_info, timeout));
}

APIResult Device::create_swap_chain(SwapChain *swap_chain, SwapChainDesc *desc, Surface *surface)
{
    ZoneScoped;

    if (!validate_handle(swap_chain))
        return APIResult::HanldeNotInitialized;

    VkSwapchainCreateInfoKHR swap_chain_info = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .pNext = nullptr,
        .surface = surface->m_handle,
        .minImageCount = desc->m_frame_count,
        .imageFormat = static_cast<VkFormat>(desc->m_format),
        .imageColorSpace = desc->m_color_space,
        .imageExtent = { desc->m_width, desc->m_height },
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,
        .preTransform = surface->get_transform(),
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = desc->m_present_mode,
        .clipped = VK_TRUE,
    };

    wait_for_work();
    auto result = static_cast<APIResult>(vkCreateSwapchainKHR(m_handle, &swap_chain_info, nullptr, &swap_chain->m_handle));
    if (result != APIResult::Success)
        return result;

    swap_chain->init(desc->m_width, desc->m_height, desc->m_frame_count, desc->m_format, desc->m_color_space, desc->m_present_mode);
    for (u32 i = 0; i < desc->m_frame_count; i++)
    {
        create_binary_semaphore(&swap_chain->m_acquire_semas[i]);
        create_binary_semaphore(&swap_chain->m_present_semas[i]);
    }

    return result;
}

void Device::delete_swap_chain(SwapChain *swap_chain)
{
    ZoneScoped;

    wait_for_work();
    vkDestroySwapchainKHR(m_handle, swap_chain->m_handle, nullptr);
    swap_chain->m_handle = VK_NULL_HANDLE;

    for (u32 i = 0; i < swap_chain->m_frame_count; i++)
    {
        delete_semaphore(&swap_chain->m_acquire_semas[i]);
        delete_semaphore(&swap_chain->m_present_semas[i]);
    }
}

APIResult Device::get_swap_chain_images(SwapChain *swap_chain, eastl::span<Image *> images)
{
    eastl::vector<VkImage> raw_images(swap_chain->m_frame_count);
    auto result = static_cast<APIResult>(vkGetSwapchainImagesKHR(m_handle, swap_chain->m_handle, &swap_chain->m_frame_count, raw_images.data()));
    if (result != APIResult::Success)
        return result;

    for (u32 i = 0; i < swap_chain->m_frame_count; i++)
    {
        Image *image = images[i];
        image->m_handle = raw_images[i];
        image->m_swap_chain_image = true;
        image->init(swap_chain->m_format, swap_chain->m_width, swap_chain->m_height, 1, 1);

        set_object_name(image, eastl::format("SwapChain Image {}", i));
    }

    return APIResult::Success;
}

void Device::wait_for_work()
{
    ZoneScoped;

    vkDeviceWaitIdle(m_handle);
}

APIResult Device::acquire_next_image(SwapChain *swap_chain, u32 &image_id)
{
    ZoneScoped;

    auto &acquire_sema = swap_chain->m_acquire_semas[swap_chain->m_current_frame_id];
    auto result = static_cast<APIResult>(vkAcquireNextImageKHR(m_handle, *swap_chain, UINT64_MAX, acquire_sema, nullptr, &image_id));
    swap_chain->m_current_frame_id = image_id;

    return result;
}

APIResult Device::present(SwapChain *swap_chain, CommandQueue *queue)
{
    ZoneScoped;

    auto &present_sema = swap_chain->m_present_semas[swap_chain->m_current_frame_id];

    VkPresentInfoKHR present_info = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &present_sema.m_handle,
        .swapchainCount = 1,
        .pSwapchains = &swap_chain->m_handle,
        .pImageIndices = &swap_chain->m_current_frame_id,
    };
    auto result = static_cast<APIResult>(vkQueuePresentKHR(*queue, &present_info));
    if (result != APIResult::Success)
        return result;

    swap_chain->m_current_frame_id = swap_chain->get_next_frame();

    return APIResult::Success;
}

PipelineLayout Device::create_pipeline_layout(eastl::span<DescriptorSetLayout> layouts, eastl::span<PushConstantDesc> push_constants)
{
    ZoneScoped;

    VkPipelineLayoutCreateInfo pipeline_layout_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .setLayoutCount = static_cast<u32>(layouts.size()),
        .pSetLayouts = layouts.data(),
        .pushConstantRangeCount = static_cast<u32>(push_constants.size()),
        .pPushConstantRanges = push_constants.data(),
    };

    PipelineLayout pLayout = nullptr;
    vkCreatePipelineLayout(m_handle, &pipeline_layout_create_info, nullptr, &pLayout);

    return pLayout;
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

    PipelineViewportStateInfo viewport_state_info(pipeline_info->m_viewports, pipeline_info->m_scissors);
    VkGraphicsPipelineCreateInfo pipeline_create_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = pipeline_attachment_info,
        .flags = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT,
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
        .layout = pipeline_info->m_layout,
    };

    pipeline->m_bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS;
    return static_cast<APIResult>(vkCreateGraphicsPipelines(m_handle, nullptr, 1, &pipeline_create_info, nullptr, &pipeline->m_handle));
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
        .layout = pipeline_info->m_layout,
    };

    pipeline->m_bind_point = VK_PIPELINE_BIND_POINT_COMPUTE;
    return static_cast<APIResult>(vkCreateComputePipelines(m_handle, nullptr, 1, &pipeline_create_info, nullptr, &pipeline->m_handle));
}

void Device::delete_pipeline(Pipeline *pipeline)
{
    ZoneScoped;

    vkDestroyPipeline(m_handle, *pipeline, nullptr);
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
    return static_cast<APIResult>(vkCreateShaderModule(m_handle, &create_info, nullptr, &shader->m_handle));
}

void Device::delete_shader(Shader *shader)
{
    ZoneScoped;

    vkDestroyShaderModule(m_handle, shader->m_handle, nullptr);
    shader->m_handle = VK_NULL_HANDLE;
}

DescriptorSetLayout Device::create_descriptor_set_layout(eastl::span<DescriptorLayoutElement> elements, DescriptorSetLayoutFlag flags)
{
    ZoneScoped;

    eastl::vector<VkDescriptorSetLayoutBinding> bindings(elements.size());
    eastl::vector<VkDescriptorBindingFlags> binding_flags(elements.size());
    for (u32 i = 0; i < elements.size(); i++)
    {
        auto &element = elements[i];
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

    DescriptorSetLayout layout = nullptr;
    vkCreateDescriptorSetLayout(m_handle, &create_info, nullptr, &layout);

    return layout;
}

void Device::delete_descriptor_set_layout(DescriptorSetLayout layout)
{
    ZoneScoped;

    vkDestroyDescriptorSetLayout(m_handle, layout, nullptr);
    layout = VK_NULL_HANDLE;
}

u64 Device::get_descriptor_set_layout_size(DescriptorSetLayout layout)
{
    ZoneScoped;

    u64 size = 0;
    vkGetDescriptorSetLayoutSizeEXT(m_handle, layout, &size);

    return size;
}

u64 Device::get_descriptor_set_layout_binding_offset(DescriptorSetLayout layout, u32 binding_id)
{
    ZoneScoped;

    u64 offset = 0;
    vkGetDescriptorSetLayoutBindingOffsetEXT(m_handle, layout, binding_id, &offset);

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
        switch (info.m_type)
        {
            case DescriptorType::StorageBuffer:
            case DescriptorType::UniformBuffer:
                bufferInfo.address = info.m_buffer_address;
                bufferInfo.range = info.m_data_size;
                descriptorData = { .pUniformBuffer = &bufferInfo };
                break;
            case DescriptorType::StorageImage:
            case DescriptorType::SampledImage:
                imageInfo.imageView = *info.m_image_view;
                descriptorData = { .pSampledImage = &imageInfo };
                break;
            case DescriptorType::Sampler:
                imageInfo.sampler = *info.m_sampler;
                descriptorData = { .pSampledImage = &imageInfo };
                break;
            default:
                break;
        }
    }

    VkDescriptorGetInfoEXT vkInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT,
        .pNext = nullptr,
        .type = (VkDescriptorType)info.m_type,
        .data = descriptorData,
    };

    vkGetDescriptorEXT(m_handle, &vkInfo, data_size, data_out);
}

APIResult Device::create_descriptor_pool(DescriptorPool *descriptor_pool, u32 max_sets, eastl::span<DescriptorPoolSize> pool_sizes)
{
    ZoneScoped;

    VkDescriptorPoolCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext = nullptr,
        .maxSets = max_sets,
        .poolSizeCount = static_cast<u32>(pool_sizes.size()),
        .pPoolSizes = pool_sizes.data(),
    };
    return static_cast<APIResult>(vkCreateDescriptorPool(m_handle, &create_info, nullptr, &descriptor_pool->m_handle));
}

void Device::delete_descriptor_pool(DescriptorPool *descriptor_pool)
{
    ZoneScoped;

    vkDestroyDescriptorPool(m_handle, *descriptor_pool, nullptr);
    descriptor_pool->m_handle = VK_NULL_HANDLE;
}

APIResult Device::create_descriptor_set(DescriptorSet *descriptor_set, eastl::span<DescriptorSetLayout> layouts, DescriptorPool *descriptor_pool)
{
    ZoneScoped;

    VkDescriptorSetVariableDescriptorCountAllocateInfo set_count_info ={
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO,
        .pNext = nullptr,

    };

    VkDescriptorSetAllocateInfo allocate_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = nullptr,
        .descriptorPool = *descriptor_pool,
        .descriptorSetCount = static_cast<u32>(layouts.size()),
        .pSetLayouts = layouts.data(),
    };
    return static_cast<APIResult>(vkAllocateDescriptorSets(m_handle, &allocate_info, &descriptor_set->m_handle));
}

void Device::delete_descriptor_set(DescriptorSet *descriptor_set, DescriptorPool *descriptor_pool)
{
    ZoneScoped;

    vkFreeDescriptorSets(m_handle, *descriptor_pool, 1, &descriptor_set->m_handle);
    descriptor_set->m_handle = VK_NULL_HANDLE;
}

eastl::tuple<u64, u64> Device::get_buffer_memory_size(Buffer *buffer)
{
    ZoneScoped;

    VkMemoryRequirements memory_requirements = {};
    vkGetBufferMemoryRequirements(m_handle, buffer->m_handle, &memory_requirements);

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

    u32 heap_index = m_physical_device->get_heap_index(static_cast<VkMemoryPropertyFlags>(desc->m_flags));
    VkMemoryAllocateInfo allocate_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = nullptr,
        .allocationSize = desc->m_size,
        .memoryTypeIndex = heap_index,
    };

    return static_cast<APIResult>(vkAllocateMemory(m_handle, &allocate_info, nullptr, &device_memory->m_handle));
}

void Device::delete_device_memory(DeviceMemory *device_memory)
{
    ZoneScoped;

    vkFreeMemory(m_handle, device_memory->m_handle, nullptr);
}

void Device::bind_memory(DeviceMemory *device_memory, Buffer *buffer, u64 memory_offset)
{
    ZoneScoped;

    vkBindBufferMemory(m_handle, buffer->m_handle, device_memory->m_handle, memory_offset);
}

void Device::bind_memory(DeviceMemory *device_memory, Image *image, u64 memory_offset)
{
    ZoneScoped;

    vkBindImageMemory(m_handle, image->m_handle, device_memory->m_handle, memory_offset);
}

APIResult Device::create_buffer(Buffer *buffer, BufferDesc *desc)
{
    ZoneScoped;

    if (!validate_handle(buffer))
        return APIResult::HanldeNotInitialized;

    u64 aligned_size = m_physical_device->get_aligned_buffer_memory(desc->m_usage_flags, desc->m_data_size);
    BufferUsage buffer_usage = desc->m_usage_flags | BufferUsage::BufferDeviceAddress;
    VkBufferCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,
        .size = aligned_size,
        .usage = static_cast<VkBufferUsageFlags>(buffer_usage),
    };
    auto result = static_cast<APIResult>(vkCreateBuffer(m_handle, &create_info, nullptr, &buffer->m_handle));
    if (result != APIResult::Success)
        return result;

    VkBufferDeviceAddressInfo device_address_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .pNext = nullptr,
        .buffer = *buffer,
    };

    /// INIT BUFFER ///
    buffer->m_stride = desc->m_stride;
    buffer->m_device_address = vkGetBufferDeviceAddress(m_handle, &device_address_info);

    return APIResult::Success;
}

void Device::delete_buffer(Buffer *buffer)
{
    ZoneScoped;

    if (!validate_handle(buffer))
        return;

    vkDestroyBuffer(m_handle, *buffer, nullptr);
    buffer->m_handle = VK_NULL_HANDLE;
}

APIResult Device::create_image(Image *image, ImageDesc *desc)
{
    ZoneScoped;

    if (!validate_handle(image))
        return APIResult::HanldeNotInitialized;

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

    auto result = static_cast<APIResult>(vkCreateImage(m_handle, &create_info, nullptr, &image->m_handle));
    if (result != APIResult::Success)
        return result;

    image->init(desc->m_format, desc->m_width, desc->m_height, desc->m_slice_count, desc->m_mip_levels);
    return APIResult::Success;
}

void Device::delete_image(Image *image)
{
    ZoneScoped;

    if (!validate_handle(image))
        return;

    if (!image->m_swap_chain_image)
        vkDestroyImage(m_handle, *image, nullptr);

    image->m_handle = VK_NULL_HANDLE;
}

APIResult Device::create_image_view(ImageView *view, ImageViewDesc *desc)
{
    ZoneScoped;

    VkImageViewCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = nullptr,
        .image = *desc->m_image,
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

    view->m_format = desc->m_image->m_format;
    view->m_type = desc->m_type;
    view->m_subresource_info = desc->m_subresource_info;

    return static_cast<APIResult>(vkCreateImageView(m_handle, &create_info, nullptr, &view->m_handle));
}

void Device::delete_image_view(ImageView *image_view)
{
    ZoneScoped;

    if (!validate_handle(image_view))
        return;

    vkDestroyImageView(m_handle, *image_view, nullptr);
    image_view->m_handle = VK_NULL_HANDLE;
}

APIResult Device::create_sampler(Sampler *sampler, SamplerDesc *desc)
{
    ZoneScoped;

    if (!validate_handle(sampler))
        return APIResult::HanldeNotInitialized;

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

    return static_cast<APIResult>(vkCreateSampler(m_handle, &create_info, nullptr, &sampler->m_handle));
}

void Device::delete_sampler(Sampler *sampler)
{
    ZoneScoped;

    if (!validate_handle(sampler))
        return;

    vkDestroySampler(m_handle, *sampler, nullptr);
    sampler->m_handle = VK_NULL_HANDLE;
}

}  // namespace lr::Graphics