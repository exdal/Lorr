#include "Device.hh"

#include "Core/Config.hh"
#include "Memory/MemoryUtils.hh"

#include "Shader.hh"

namespace lr::Graphics
{
Device::Device(PhysicalDevice *physical_device, VkDevice handle)
    : m_physical_device(physical_device),
      m_handle(handle)
{
    m_pipeline_cache = create_pipeline_cache();
    m_tracy_ctx = TracyVkContextHostCalibrated(pPhysicalDevice->m_pHandle, m_pHandle);
}

CommandQueue *Device::create_command_queue(CommandType type, u32 queue_index)
{
    ZoneScoped;

    CommandQueue *pQueue = new CommandQueue;
    pQueue->m_type = type;
    pQueue->m_queue_index = queue_index;
    vkGetDeviceQueue(m_handle, queue_index, 0, &pQueue->m_handle);

    return pQueue;
}

CommandAllocator *Device::create_command_allocator(u32 queue_idx, CommandAllocatorFlag flags)
{
    ZoneScoped;

    CommandAllocator *pAllocator = new CommandAllocator;

    VkCommandPoolCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = static_cast<VkCommandPoolCreateFlags>(flags),
        .queueFamilyIndex = queue_idx,
    };
    vkCreateCommandPool(m_handle, &createInfo, nullptr, &pAllocator->m_handle);

    return pAllocator;
}

CommandList *Device::create_command_list(CommandAllocator *command_allocator)
{
    ZoneScoped;

    CommandList *pList = new CommandList;

    VkCommandBufferAllocateInfo listInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = command_allocator->m_handle,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    vkAllocateCommandBuffers(m_handle, &listInfo, &pList->m_handle);

    return pList;
}

void Device::begin_command_list(CommandList *list)
{
    ZoneScoped;

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.pNext = nullptr;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(list->m_handle, &beginInfo);
}

void Device::end_command_list(CommandList *list)
{
    ZoneScoped;

    TracyVkCollect(m_pTracyCtx, pList->m_pHandle);
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

    VkSubmitInfo2 submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .waitSemaphoreInfoCount = (u32)desc->m_wait_semas.size(),
        .pWaitSemaphoreInfos = desc->m_wait_semas.data(),
        .commandBufferInfoCount = (u32)desc->m_lists.size(),
        .pCommandBufferInfos = desc->m_lists.data(),
        .signalSemaphoreInfoCount = (u32)desc->m_signal_semas.size(),
        .pSignalSemaphoreInfos = desc->m_signal_semas.begin(),
    };

    vkQueueSubmit2(queue->m_handle, 1, &submitInfo, nullptr);
}

Fence Device::create_fence(bool signaled)
{
    ZoneScoped;

    Fence pFence = nullptr;

    VkFenceCreateInfo fenceInfo = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = signaled,
    };
    vkCreateFence(m_handle, &fenceInfo, nullptr, &pFence);

    return pFence;
}

void Device::delete_fence(Fence fence)
{
    ZoneScoped;

    vkDestroyFence(m_handle, fence, nullptr);
}

bool Device::is_fence_signaled(Fence fence)
{
    ZoneScoped;

    return vkGetFenceStatus(m_handle, fence) == VK_SUCCESS;
}

void Device::reset_fence(Fence fence)
{
    ZoneScoped;

    vkResetFences(m_handle, 1, &fence);
}

Semaphore *Device::create_binary_semaphore()
{
    ZoneScoped;

    Semaphore *pSemaphore = new Semaphore;
    pSemaphore->m_value = 0;

    VkSemaphoreTypeCreateInfo semaphoreTypeInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
        .semaphoreType = VK_SEMAPHORE_TYPE_BINARY,
        .initialValue = pSemaphore->m_value,
    };

    VkSemaphoreCreateInfo semaphoreInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = &semaphoreTypeInfo,
    };
    vkCreateSemaphore(m_handle, &semaphoreInfo, nullptr, &pSemaphore->m_handle);

    return pSemaphore;
}

Semaphore *Device::create_timeline_semaphore(u64 initial_value)
{
    ZoneScoped;

    Semaphore *pSemaphore = new Semaphore;
    pSemaphore->m_value = initial_value;

    VkSemaphoreTypeCreateInfo semaphoreTypeInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
        .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
        .initialValue = pSemaphore->m_value,
    };

    VkSemaphoreCreateInfo semaphoreInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = &semaphoreTypeInfo,
    };
    vkCreateSemaphore(m_handle, &semaphoreInfo, nullptr, &pSemaphore->m_handle);

    return pSemaphore;
}

void Device::delete_semaphore(Semaphore *semaphore)
{
    ZoneScoped;

    vkDestroySemaphore(m_handle, semaphore->m_handle, nullptr);
    delete semaphore;
}

void Device::wait_for_semaphore(Semaphore *semaphore, u64 desired_value, u64 timeout)
{
    ZoneScoped;

    VkSemaphoreWaitInfo waitInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
        .semaphoreCount = 1,
        .pSemaphores = &semaphore->m_handle,
        .pValues = &desired_value,
    };
    vkWaitSemaphores(m_handle, &waitInfo, timeout);
}

VkPipelineCache Device::create_pipeline_cache(u32 initial_data_size, void *initial_data)
{
    ZoneScoped;

    VkPipelineCache pHandle = nullptr;

    VkPipelineCacheCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
        .initialDataSize = initial_data_size,
        .pInitialData = initial_data,
    };
    vkCreatePipelineCache(m_handle, &createInfo, nullptr, &pHandle);

    return pHandle;
}

PipelineLayout Device::create_pipeline_layout(eastl::span<DescriptorSetLayout> layouts, eastl::span<PushConstantDesc> push_constants)
{
    ZoneScoped;

    VkPipelineLayoutCreateInfo pipeline_layout_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .setLayoutCount = (u32)layouts.size(),
        .pSetLayouts = layouts.data(),
        .pushConstantRangeCount = (u32)push_constants.size(),
        .pPushConstantRanges = push_constants.data(),
    };

    PipelineLayout pLayout = nullptr;
    vkCreatePipelineLayout(m_handle, &pipeline_layout_create_info, nullptr, &pLayout);

    return pLayout;
}

Pipeline *Device::create_graphics_pipeline(GraphicsPipelineInfo *pipeline_info, PipelineAttachmentInfo *pipeline_attachment_info)
{
    ZoneScoped;

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

    /// VIEWPORT -----------------------------------------------------------------

    VkPipelineViewportStateCreateInfo viewport_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pNext = nullptr,
        .viewportCount = 1,
        .pViewports = nullptr,
        .scissorCount = 1,
        .pScissors = nullptr,
    };

    /// RASTERIZER ---------------------------------------------------------------

    VkPipelineRasterizationStateCreateInfo rasterizer_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .pNext = nullptr,
        .depthClampEnable = pipeline_info->m_enable_depth_clamp,
        .polygonMode = static_cast<VkPolygonMode>(pipeline_info->m_fill_mode),
        .cullMode = static_cast<VkCullModeFlags>(pipeline_info->m_cull_mode),
        .frontFace = static_cast<VkFrontFace>(!pipeline_info->m_front_face_ccw),
        .depthBiasEnable = pipeline_info->m_enable_depth_bias,
        .depthBiasConstantFactor = pipeline_info->m_depth_bias_factor,
        .depthBiasClamp = pipeline_info->m_depth_bias_clamp,
        .depthBiasSlopeFactor = pipeline_info->m_depth_slope_factor,
        .lineWidth = 1.0,
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

    VkGraphicsPipelineCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = pipeline_attachment_info,
        .flags = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT,
        .stageCount = static_cast<u32>(pipeline_info->m_shader_stages.size()),
        .pStages = pipeline_info->m_shader_stages.data(),
        .pVertexInputState = &input_layout_info,
        .pInputAssemblyState = &input_assembly_info,
        .pTessellationState = &tessellation_info,
        .pViewportState = &viewport_info,
        .pRasterizationState = &rasterizer_info,
        .pMultisampleState = &multisample_info,
        .pDepthStencilState = &depth_stencil_info,
        .pColorBlendState = &color_blend_info,
        .pDynamicState = &dynamic_state_info,
        .layout = pipeline_info->m_layout,
    };

    VkPipeline pipeline_handle = nullptr;
    vkCreateGraphicsPipelines(m_handle, m_pipeline_cache, 1, &createInfo, nullptr, &pipeline_handle);

    return new Pipeline(pipeline_handle);
}

Pipeline *Device::create_compute_pipeline(ComputePipelineInfo *pipeline_info)
{
    ZoneScoped;

    VkComputePipelineCreateInfo pipeline_create_info = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .pNext = nullptr,
        .stage = pipeline_info->m_shader_stage,
        .layout = pipeline_info->m_layout,
    };

    VkPipeline pipeline_handle = nullptr;
    vkCreateComputePipelines(m_handle, m_pipeline_cache, 1, &pipeline_create_info, nullptr, &pipeline_handle);

    return new Pipeline(pipeline_handle);
}

SwapChain *Device::create_swap_chain(Surface *surface, SwapChainDesc *desc)
{
    ZoneScoped;

    SwapChain *pSwapChain = new SwapChain;
    pSwapChain->m_frame_count = desc->m_frame_count;
    pSwapChain->m_width = desc->m_width;
    pSwapChain->m_height = desc->m_height;
    pSwapChain->m_image_format = desc->m_format;
    pSwapChain->m_color_space = desc->m_color_space;
    pSwapChain->m_present_mode = desc->m_present_mode;

    VkSwapchainCreateInfoKHR swapChainInfo = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .pNext = nullptr,
        .surface = surface->m_handle,
        .minImageCount = pSwapChain->m_frame_count,
        .imageFormat = (VkFormat)pSwapChain->m_image_format,
        .imageColorSpace = pSwapChain->m_color_space,
        .imageExtent = { pSwapChain->m_width, pSwapChain->m_height },
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,
        .preTransform = surface->get_transform(),
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = pSwapChain->m_present_mode,
        .clipped = VK_TRUE,
    };

    wait_for_work();
    auto result = static_cast<APIResult>(vkCreateSwapchainKHR(m_handle, &swapChainInfo, nullptr, &pSwapChain->m_handle));
    if (result != APIResult::Success)
    {
        LOG_ERROR("Failed to create SwapChain! {}", static_cast<i32>(result));
        delete pSwapChain;
        return nullptr;
    }

    return pSwapChain;
}

void Device::delete_swap_chain(SwapChain *swap_chain)
{
    ZoneScoped;

    wait_for_work();
    vkDestroySwapchainKHR(m_handle, swap_chain->m_handle, nullptr);
    delete swap_chain;
}

ls::ref_array<Image *> Device::get_swap_chain_images(SwapChain *swap_chain)
{
    ZoneScoped;

    u32 imageCount = swap_chain->m_frame_count;
    ls::ref_array ppImages(new Image *[imageCount]);
    ls::ref_array ppImageHandles(new VkImage[imageCount]);
    vkGetSwapchainImagesKHR(m_handle, swap_chain->m_handle, &imageCount, ppImageHandles.get());

    for (u32 i = 0; i < imageCount; i++)
    {
        Image *&pImage = ppImages[i];
        pImage = new Image;
        pImage->m_handle = ppImageHandles[i];
        pImage->m_width = swap_chain->m_width;
        pImage->m_height = swap_chain->m_height;
        pImage->m_data_size = ~0;
        pImage->m_data_offset = ~0;
        pImage->m_mip_map_levels = 1;
        pImage->m_format = swap_chain->m_image_format;

        SetObjectName(pImage, _FMT("Swap Chain Image {}", i));
    }

    return ppImages;
}

void Device::wait_for_work()
{
    ZoneScoped;

    vkDeviceWaitIdle(m_handle);
}

APIResult Device::acquire_image(SwapChain *swap_chain, Semaphore *semaphore, u32 &image_idx)
{
    ZoneScoped;

    return static_cast<APIResult>(vkAcquireNextImageKHR(m_handle, *swap_chain, UINT64_MAX, *semaphore, nullptr, &image_idx));
}

void Device::Present(SwapChain *swap_chain, u32 image_idx, Semaphore *semaphore, CommandQueue *queue)
{
    ZoneScoped;

    VkPresentInfoKHR presentInfo = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &semaphore->m_handle,
        .swapchainCount = 1,
        .pSwapchains = &swap_chain->m_handle,
        .pImageIndices = &image_idx,
    };
    vkQueuePresentKHR(queue->m_handle, &presentInfo);
}

u64 Device::get_buffer_memory_size(Buffer *buffer, u64 *alignment_out)
{
    ZoneScoped;

    VkMemoryRequirements memoryRequirements = {};
    vkGetBufferMemoryRequirements(m_handle, buffer->m_handle, &memoryRequirements);

    if (alignment_out)
        *alignment_out = memoryRequirements.alignment;

    return memoryRequirements.size;
}

u64 Device::get_image_memory_size(Image *image, u64 *alignment_out)
{
    ZoneScoped;

    VkMemoryRequirements memoryRequirements = {};
    vkGetImageMemoryRequirements(m_handle, image->m_handle, &memoryRequirements);

    if (alignment_out)
        *alignment_out = memoryRequirements.alignment;

    return memoryRequirements.size;
}

DeviceMemory *Device::create_device_memory(DeviceMemoryDesc *desc, PhysicalDevice *physical_device)
{
    ZoneScoped;

    u32 heapIndex = physical_device->get_heap_index((VkMemoryPropertyFlags)desc->m_flags);
    DeviceMemory *pDeviceMemory = nullptr;
    switch (desc->m_type)
    {
        case AllocatorType::Linear:
            pDeviceMemory = new LinearDeviceMemory;
            break;
        case AllocatorType::TLSF:
            pDeviceMemory = new TLSFDeviceMemory;
            break;
    }

    VkMemoryAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = nullptr,
        .allocationSize = desc->m_size,
        .memoryTypeIndex = heapIndex,
    };
    vkAllocateMemory(m_handle, &allocInfo, nullptr, &pDeviceMemory->m_handle);

    pDeviceMemory->create(desc->m_size, desc->m_max_allocations);
    if (desc->m_flags & MemoryFlag::HostVisible)
        vkMapMemory(m_handle, pDeviceMemory->m_handle, 0, VK_WHOLE_SIZE, 0, &pDeviceMemory->m_mapped_memory);

    return pDeviceMemory;
}

void Device::delete_device_memory(DeviceMemory *device_memory)
{
    ZoneScoped;

    vkFreeMemory(m_handle, device_memory->m_handle, nullptr);
    delete device_memory;
}

void Device::allocate_buffer_memory(DeviceMemory *device_memory, Buffer *buffer, u64 memory_size)
{
    ZoneScoped;

    u64 alignment = 0;
    u64 alignedSize = get_buffer_memory_size(buffer, &alignment);
    u64 memoryOffset = device_memory->allocate_memory(alignedSize, alignment, buffer->m_allocator_data);

    vkBindBufferMemory(m_handle, buffer->m_handle, device_memory->m_handle, memoryOffset);

    buffer->m_data_size = alignedSize;
    buffer->m_data_offset = memoryOffset;
}

void Device::allocate_image_memory(DeviceMemory *device_memory, Image *image, u64 memory_size)
{
    ZoneScoped;

    u64 alignment = 0;
    u64 alignedSize = get_image_memory_size(image, &alignment);
    u64 memoryOffset = device_memory->allocate_memory(alignedSize, alignment, image->m_allocator_data);

    vkBindImageMemory(m_handle, image->m_handle, device_memory->m_handle, memoryOffset);

    image->m_data_size = alignedSize;
    image->m_data_offset = memoryOffset;
}

Shader *Device::create_shader(ShaderStage stage, u32 *data, u64 data_size)
{
    ZoneScoped;

    Shader *pShader = new Shader;
    pShader->m_type = stage;

    VkShaderModuleCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = data_size,
        .pCode = data,
    };
    vkCreateShaderModule(m_handle, &createInfo, nullptr, &pShader->m_handle);

    return pShader;
}

void Device::delete_shader(Shader *shader)
{
    ZoneScoped;

    vkDestroyShaderModule(m_handle, shader->m_handle, nullptr);
    delete shader;
}

DescriptorSetLayout Device::create_descriptor_set_layout(eastl::span<DescriptorLayoutElement> elements, DescriptorSetLayoutFlag flags)
{
    ZoneScoped;

    DescriptorSetLayout pLayout = nullptr;

    VkDescriptorSetLayoutCreateFlags createFlags = 0;
    if (flags & DescriptorSetLayoutFlag::DescriptorBuffer)
        createFlags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
    if (flags & DescriptorSetLayoutFlag::EmbeddedSamplers)
        createFlags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_EMBEDDED_IMMUTABLE_SAMPLERS_BIT_EXT;

    VkDescriptorSetLayoutCreateInfo layoutCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = createFlags,
        .bindingCount = (u32)elements.size(),
        .pBindings = elements.data(),
    };

    vkCreateDescriptorSetLayout(m_handle, &layoutCreateInfo, nullptr, &pLayout);

    return pLayout;
}

void Device::delete_descriptor_set_layout(DescriptorSetLayout layout)
{
    ZoneScoped;

    vkDestroyDescriptorSetLayout(m_handle, layout, nullptr);
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

Buffer *Device::create_buffer(BufferDesc *desc, DeviceMemory *device_memory)
{
    ZoneScoped;

    Buffer *pBuffer = new Buffer;
    u64 alignedSize = m_physical_device->get_aligned_buffer_memory(desc->m_usage_flags, desc->m_data_size);

    VkBufferCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,
        .size = alignedSize,
        .usage = static_cast<VkBufferUsageFlags>(desc->m_usage_flags) | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
    };
    vkCreateBuffer(m_handle, &createInfo, nullptr, &pBuffer->m_handle);
    allocate_buffer_memory(device_memory, pBuffer, alignedSize);

    VkBufferDeviceAddressInfo deviceAddressInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .pNext = nullptr,
        .buffer = pBuffer->m_handle,
    };

    /// INIT BUFFER ///
    pBuffer->m_stride = desc->m_stride;
    pBuffer->m_device_address = vkGetBufferDeviceAddress(m_handle, &deviceAddressInfo);

    return pBuffer;
}

void Device::delete_buffer(Buffer *pBuffer, DeviceMemory *device_memory)
{
    ZoneScoped;

    if (device_memory)
        device_memory->free_memory(pBuffer->m_allocator_data);

    if (pBuffer->m_handle)
        vkDestroyBuffer(m_handle, pBuffer->m_handle, nullptr);
}

u8 *Device::get_memory_data(DeviceMemory *device_memory)
{
    ZoneScoped;

    return (u8 *)device_memory->m_mapped_memory;
}

u8 *Device::get_buffer_memory_data(DeviceMemory *device_memory, Buffer *buffer)
{
    ZoneScoped;

    return get_memory_data(device_memory) + buffer->m_data_offset;
}

Image *Device::create_image(ImageDesc *desc, DeviceMemory *device_memory)
{
    ZoneScoped;

    Image *pImage = new Image;

    if (desc->m_data_size == 0)
        desc->m_data_size = desc->m_width * desc->m_height * FormatToSize(desc->m_format);

    /// ---------------------------------------------- //

    VkImageCreateInfo imageCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = nullptr,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = (VkFormat)desc->m_format,
        .extent.width = desc->m_width,
        .extent.height = desc->m_height,
        .extent.depth = 1,
        .mipLevels = desc->m_mip_map_levels,
        .arrayLayers = desc->m_array_size,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .usage = (VkImageUsageFlags)desc->m_usage_flags,
        .sharingMode = desc->m_usage_flags & ImageUsage::Concurrent ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = (u32)desc->m_queue_indices.size(),
        .pQueueFamilyIndices = desc->m_queue_indices.data(),
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    vkCreateImage(m_handle, &imageCreateInfo, nullptr, &pImage->m_handle);
    allocate_image_memory(device_memory, pImage, desc->m_data_size);

    /// INIT BUFFER ///
    pImage->m_format = desc->m_format;
    pImage->m_width = desc->m_width;
    pImage->m_height = desc->m_height;
    pImage->m_array_size = desc->m_array_size;
    pImage->m_mip_map_levels = desc->m_mip_map_levels;

    return pImage;
}

void Device::delete_image(Image *image, DeviceMemory *device_memory)
{
    ZoneScoped;

    if (device_memory)
        device_memory->free_memory(image->m_allocator_data);

    vkDestroyImage(m_handle, *image, nullptr);
    delete image;
}

ImageView *Device::create_image_view(ImageViewDesc *desc)
{
    ZoneScoped;

    ImageView *pImageView = new ImageView;

    VkImageViewCreateInfo imageViewCreateInfo = {
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

    vkCreateImageView(m_handle, &imageViewCreateInfo, nullptr, &pImageView->m_handle);

    pImageView->m_format = desc->m_image->m_format;
    pImageView->m_type = desc->m_type;
    pImageView->m_subresource_info = desc->m_subresource_info;

    return pImageView;
}

void Device::delete_image_view(ImageView *image_view)
{
    ZoneScoped;

    vkDestroyImageView(m_handle, *image_view, nullptr);
    delete image_view;
}

Sampler *Device::create_sampler(SamplerDesc *desc)
{
    ZoneScoped;

    Sampler *pSampler = new Sampler;

    VkSamplerCreateInfo samplerInfo = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .magFilter = (VkFilter)desc->m_min_filter,
        .minFilter = (VkFilter)desc->m_min_filter,
        .mipmapMode = (VkSamplerMipmapMode)desc->m_mip_filter,
        .addressModeU = (VkSamplerAddressMode)desc->m_address_u,
        .addressModeV = (VkSamplerAddressMode)desc->m_address_v,
        .addressModeW = (VkSamplerAddressMode)desc->m_address_w,
        .mipLodBias = desc->m_mip_lod_bias,
        .anisotropyEnable = desc->m_use_anisotropy,
        .maxAnisotropy = desc->m_max_anisotropy,
        .compareEnable = desc->m_compare_op != CompareOp::Never,
        .compareOp = (VkCompareOp)desc->m_compare_op,
        .minLod = desc->m_min_lod,
        .maxLod = desc->m_max_lod,
        .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK,
    };
    vkCreateSampler(m_handle, &samplerInfo, nullptr, &pSampler->m_handle);

    return pSampler;
}

void Device::delete_sampler(VkSampler sampler)
{
    ZoneScoped;

    vkDestroySampler(m_handle, sampler, nullptr);
}

}  // namespace lr::Graphics