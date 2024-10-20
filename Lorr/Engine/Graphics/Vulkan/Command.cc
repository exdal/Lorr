#include "Engine/Graphics/Vulkan/Impl.hh"

#include "Engine/Graphics/Vulkan/ToVk.hh"

namespace lr {
auto Semaphore::create(Device_H device, ls::option<u64> initial_value, const std::string &name) -> std::expected<Semaphore, vk::Result> {
    ZoneScoped;

    auto impl = new Impl;
    impl->device = device;

    VkSemaphoreTypeCreateInfo semaphore_type_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
        .pNext = nullptr,
        .semaphoreType = initial_value.has_value() ? VK_SEMAPHORE_TYPE_BINARY : VK_SEMAPHORE_TYPE_TIMELINE,
        .initialValue = initial_value.value_or(0),
    };

    VkSemaphoreCreateInfo semaphore_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = &semaphore_type_info,
        .flags = 0,
    };

    auto result = vkCreateSemaphore(device->handle, &semaphore_info, nullptr, &impl->handle);
    switch (result) {
        case VK_ERROR_OUT_OF_HOST_MEMORY:
            return std::unexpected(vk::Result::OutOfHostMem);
        case VK_ERROR_OUT_OF_DEVICE_MEMORY:
            return std::unexpected(vk::Result::OutOfDeviceMem);
        default:;
    }

    set_object_name(device, impl->handle, VK_OBJECT_TYPE_SEMAPHORE, name);

    return Semaphore(impl);
}

auto Semaphore::destroy() -> void {
    ZoneScoped;

    vkDestroySemaphore(impl->device->handle, impl->handle, nullptr);
}

auto Semaphore::advance() -> u64 {
    return ++impl->value;
}

auto Semaphore::counter() -> u64 {
    return impl->value;
}

auto QueryPool::create(Device_H device, u32 query_count, const std::string &name) -> std::expected<QueryPool, vk::Result> {
    ZoneScoped;

    // We are getting query results with availibilty bit
    LS_EXPECT((query_count & 1) == 0);

    auto impl = new Impl;
    impl->device = device;

    VkQueryPoolCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .queryType = VK_QUERY_TYPE_TIMESTAMP,
        .queryCount = query_count,
        .pipelineStatistics = {},
    };

    auto result = vkCreateQueryPool(device->handle, &create_info, nullptr, &impl->handle);
    switch (result) {
        case VK_ERROR_OUT_OF_HOST_MEMORY:
            return std::unexpected(vk::Result::OutOfHostMem);
        case VK_ERROR_OUT_OF_DEVICE_MEMORY:
            return std::unexpected(vk::Result::OutOfDeviceMem);
        default:;
    }

    set_object_name(device, impl->handle, VK_OBJECT_TYPE_QUERY_POOL, name);

    // This is literally just memset to zero, cmd version is same but happens when queue is on execute state
    vkResetQueryPool(device->handle, impl->handle, 0, query_count);

    return QueryPool(impl);
}

auto QueryPool::destroy() -> void {
    vkDestroyQueryPool(impl->device->handle, impl->handle, nullptr);
}

auto QueryPool::get_results(u32 first_query, u32 count, ls::span<u64> time_stamps) -> void {
    ZoneScoped;

    vkGetQueryPoolResults(
        impl->device->handle,
        impl->handle,
        first_query,
        count,
        time_stamps.size() * sizeof(u64),
        time_stamps.data(),
        2 * sizeof(u64),
        VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);
}

auto CommandList::reset_query_pool(QueryPool_H query_pool, u32 first_query, u32 query_count) -> void {
    ZoneScoped;

    vkCmdResetQueryPool(impl->handle, query_pool->handle, first_query, query_count);
}

auto CommandList::write_query_pool(QueryPool_H query_pool, vk::PipelineStage pipeline_stage, u32 query_index) -> void {
    ZoneScoped;

    vkCmdWriteTimestamp2(impl->handle, std::to_underlying(pipeline_stage), query_pool->handle, query_index);
}

auto CommandList::image_transition(const vk::ImageBarrier &barrier) -> void {
    ZoneScoped;

    auto image = impl->device.image(barrier.image_id);
    auto subresource = image.subresource_range();
    VkImageSubresourceRange vk_image_subresource_range = {
        .aspectMask = to_vk_image_aspect_flags(subresource.aspect_flags),
        .baseMipLevel = subresource.base_mip,
        .levelCount = subresource.mip_count,
        .baseArrayLayer = subresource.base_slice,
        .layerCount = subresource.slice_count,
    };

    VkImageMemoryBarrier2 image_memory_barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .pNext = nullptr,
        .srcStageMask = std::to_underlying(barrier.src_stage),
        .srcAccessMask = std::to_underlying(barrier.src_access),
        .dstStageMask = std::to_underlying(barrier.dst_stage),
        .dstAccessMask = std::to_underlying(barrier.dst_access),
        .oldLayout = static_cast<VkImageLayout>(barrier.old_layout),
        .newLayout = static_cast<VkImageLayout>(barrier.new_layout),
        .srcQueueFamilyIndex = ~0_u32,
        .dstQueueFamilyIndex = ~0_u32,
        .image = image->handle,
        .subresourceRange = vk_image_subresource_range,
    };

    VkDependencyInfo dependency_info = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .pNext = nullptr,
        .dependencyFlags = 0,
        .memoryBarrierCount = 0,
        .pMemoryBarriers = nullptr,
        .bufferMemoryBarrierCount = 0,
        .pBufferMemoryBarriers = nullptr,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &image_memory_barrier,
    };
    vkCmdPipelineBarrier2(impl->handle, &dependency_info);
}

auto CommandList::memory_barrier(const vk::MemoryBarrier &barrier) -> void {
    ZoneScoped;

    VkMemoryBarrier2 memory_barrier = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .pNext = nullptr,
        .srcStageMask = std::to_underlying(barrier.src_stage),
        .srcAccessMask = std::to_underlying(barrier.src_access),
        .dstStageMask = std::to_underlying(barrier.dst_stage),
        .dstAccessMask = std::to_underlying(barrier.dst_access),
    };

    VkDependencyInfo dependency_info = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .pNext = nullptr,
        .dependencyFlags = 0,
        .memoryBarrierCount = 1,
        .pMemoryBarriers = &memory_barrier,
        .bufferMemoryBarrierCount = 0,
        .pBufferMemoryBarriers = nullptr,
        .imageMemoryBarrierCount = 0,
        .pImageMemoryBarriers = nullptr,
    };
    vkCmdPipelineBarrier2(impl->handle, &dependency_info);
}

auto CommandList::set_barriers(ls::span<vk::MemoryBarrier> memory_barriers, ls::span<vk::ImageBarrier> image_barriers) -> void {
    ZoneScoped;
    memory::ScopedStack stack;

    auto vk_memory_barriers = stack.alloc<VkMemoryBarrier2>(memory_barriers.size());
    auto vk_image_barriers = stack.alloc<VkImageMemoryBarrier2>(image_barriers.size());

    for (u32 i = 0; i < memory_barriers.size(); i++) {
        auto &v = memory_barriers[i];
        vk_memory_barriers[i] = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
            .pNext = nullptr,
            .srcStageMask = std::to_underlying(v.src_stage),
            .srcAccessMask = std::to_underlying(v.src_access),
            .dstStageMask = std::to_underlying(v.dst_stage),
            .dstAccessMask = std::to_underlying(v.dst_access),
        };
    }

    for (u32 i = 0; i < image_barriers.size(); i++) {
        auto &v = image_barriers[i];
        auto image = impl->device.image(v.image_id);
        auto subresource = image.subresource_range();

        VkImageSubresourceRange vk_image_subresource_range = {
            .aspectMask = to_vk_image_aspect_flags(subresource.aspect_flags),
            .baseMipLevel = subresource.base_mip,
            .levelCount = subresource.mip_count,
            .baseArrayLayer = subresource.base_slice,
            .layerCount = subresource.slice_count,
        };

        vk_image_barriers[i] = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .pNext = nullptr,
            .srcStageMask = std::to_underlying(v.src_stage),
            .srcAccessMask = std::to_underlying(v.src_access),
            .dstStageMask = std::to_underlying(v.dst_stage),
            .dstAccessMask = std::to_underlying(v.dst_access),
            .oldLayout = static_cast<VkImageLayout>(v.old_layout),
            .newLayout = static_cast<VkImageLayout>(v.new_layout),
            .srcQueueFamilyIndex = ~0_u32,
            .dstQueueFamilyIndex = ~0_u32,
            .image = image->handle,
            .subresourceRange = vk_image_subresource_range,
        };
    }

    VkDependencyInfo dependency_info = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .pNext = nullptr,
        .dependencyFlags = 0,
        .memoryBarrierCount = static_cast<u32>(vk_memory_barriers.size()),
        .pMemoryBarriers = vk_memory_barriers.data(),
        .bufferMemoryBarrierCount = 0,
        .pBufferMemoryBarriers = nullptr,
        .imageMemoryBarrierCount = static_cast<u32>(vk_image_barriers.size()),
        .pImageMemoryBarriers = vk_image_barriers.data(),
    };
    vkCmdPipelineBarrier2(impl->handle, &dependency_info);
}

auto CommandList::copy_buffer_to_buffer(BufferID src_id, BufferID dst_id, ls::span<vk::BufferCopyRegion> regions) -> void {
    ZoneScoped;
    memory::ScopedStack stack;

    auto src_buffer = impl->device.buffer(src_id);
    auto dst_buffer = impl->device.buffer(dst_id);
    auto vk_regions = stack.alloc<VkBufferCopy>(regions.size());
    for (u32 i = 0; i < vk_regions.size(); i++) {
        auto &v = regions[i];
        vk_regions[i] = { .srcOffset = v.src_offset, .dstOffset = v.dst_offset, .size = v.size };
    }

    vkCmdCopyBuffer(impl->handle, src_buffer->handle, dst_buffer->handle, regions.size(), vk_regions.data());
}

auto CommandList::copy_buffer_to_image(BufferID src_id, ImageID dst_id, vk::ImageLayout layout, ls::span<vk::ImageCopyRegion> regions)
    -> void {
    ZoneScoped;
    memory::ScopedStack stack;

    auto src_buffer = impl->device.buffer(src_id);
    auto dst_image = impl->device.image(dst_id);
    auto vk_regions = stack.alloc<VkBufferImageCopy>(regions.size());
    for (u32 i = 0; i < vk_regions.size(); i++) {
        auto &v = regions[i];

        VkImageSubresourceLayers vk_subres_layer = {
            .aspectMask = to_vk_image_aspect_flags(v.image_subresource_layer.aspect_flags),
            .mipLevel = v.image_subresource_layer.target_mip,
            .baseArrayLayer = v.image_subresource_layer.base_slice,
            .layerCount = v.image_subresource_layer.slice_count,
        };

        VkOffset3D image_offset = { v.image_offset.x, v.image_offset.y, v.image_offset.z };
        VkExtent3D image_extent = { v.image_extent.width, v.image_extent.height, v.image_extent.depth };
        vk_regions[i] = {
            .bufferOffset = v.buffer_offset,
            .bufferRowLength = v.buffer_row_length,
            .bufferImageHeight = v.buffer_image_height,
            .imageSubresource = vk_subres_layer,
            .imageOffset = image_offset,
            .imageExtent = image_extent,
        };
    }

    vkCmdCopyBufferToImage(
        impl->handle, src_buffer->handle, dst_image->handle, static_cast<VkImageLayout>(layout), regions.size(), vk_regions.data());
}

auto CommandList::blit_image(
    ImageID src_id,
    vk::ImageLayout src_layout,
    ImageID dst_id,
    vk::ImageLayout dst_layout,
    vk::Filtering filter,
    ls::span<vk::ImageBlit> blits) -> void {
    ZoneScoped;
    memory::ScopedStack stack;

    auto vk_blits = stack.alloc<VkImageBlit2>(blits.size());
    for (u32 i = 0; i < vk_blits.size(); i++) {
        auto &v = blits[i];
        auto &vk_blit = vk_blits[i];

        vk_blit.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2;
        vk_blit.pNext = nullptr;
        vk_blit.srcSubresource = {
            .aspectMask = to_vk_image_aspect_flags(v.src_subresource.aspect_flags),
            .mipLevel = v.src_subresource.target_mip,
            .baseArrayLayer = v.src_subresource.base_slice,
            .layerCount = v.src_subresource.slice_count,
        };
        vk_blit.srcOffsets[0] = { v.src_offsets[0].x, v.src_offsets[0].y, v.src_offsets[0].z };
        vk_blit.srcOffsets[1] = { v.src_offsets[1].x, v.src_offsets[1].y, v.src_offsets[1].z };
        vk_blit.dstSubresource = {
            .aspectMask = to_vk_image_aspect_flags(v.dst_subresource.aspect_flags),
            .mipLevel = v.dst_subresource.target_mip,
            .baseArrayLayer = v.dst_subresource.base_slice,
            .layerCount = v.dst_subresource.slice_count,
        };
        vk_blit.dstOffsets[0] = { v.dst_offsets[0].x, v.dst_offsets[0].y, v.dst_offsets[0].z };
        vk_blit.dstOffsets[1] = { v.dst_offsets[1].x, v.dst_offsets[1].y, v.dst_offsets[1].z };
    };

    VkBlitImageInfo2 blit_info = {
        .sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
        .pNext = nullptr,
        .srcImage = impl->device.image(src_id)->handle,
        .srcImageLayout = static_cast<VkImageLayout>(src_layout),
        .dstImage = impl->device.image(dst_id)->handle,
        .dstImageLayout = static_cast<VkImageLayout>(dst_layout),
        .regionCount = static_cast<u32>(vk_blits.size()),
        .pRegions = vk_blits.data(),
        .filter = to_vk_filtering(filter),
    };
    vkCmdBlitImage2(impl->handle, &blit_info);
}

auto CommandList::begin_rendering(const RenderingBeginInfo &info) -> void {
    ZoneScoped;
    memory::ScopedStack stack;

    auto color_attachments = stack.alloc<VkRenderingAttachmentInfo>(info.color_attachments.size());
    VkRenderingAttachmentInfo depth_attachment = {};
    VkRenderingAttachmentInfo stencil_attachment = {};

    for (u32 i = 0; i < color_attachments.size(); i++) {
        auto &v = info.color_attachments[i];
        auto &vk_info = color_attachments[i];
        auto image_view = impl->device.image_view(v.image_view_id);

        vk_info = {
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .pNext = nullptr,
        };
    }

    if (info.depth_attachment) {
        ImageView &image_view = self.device->image_view_at(info.depth_attachment->image_view_id);
        depth_attachment = info.depth_attachment->vk_info(image_view);
    }

    if (info.stencil_attachment) {
        ImageView &image_view = self.device->image_view_at(info.stencil_attachment->image_view_id);
        stencil_attachment = info.stencil_attachment->vk_info(image_view);
    }

    VkRenderingInfo rendering_info = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .pNext = nullptr,
        .flags = 0,
        .renderArea = info.render_area,
        .layerCount = 1,
        .viewMask = 0,
        .colorAttachmentCount = static_cast<u32>(color_attachments.size()),
        .pColorAttachments = color_attachments.data(),
        .pDepthAttachment = info.depth_attachment ? &depth_attachment : nullptr,
        .pStencilAttachment = info.stencil_attachment ? &stencil_attachment : nullptr,
    };
    vkCmdBeginRendering(impl->handle, &rendering_info);
}

auto CommandList::end_rendering() -> void {
}

auto CommandList::set_pipeline(PipelineID pipeline_id) -> void {
}

auto CommandList::set_push_constants(void *data, u32 data_size, u32 data_offset) -> void {
}

auto CommandList::set_descriptor_set() -> void {
}

auto CommandList::set_vertex_buffer(BufferID buffer_id, u64 offset, u32 first_binding, u32 binding_count) -> void {
}

auto CommandList::set_index_buffer(BufferID buffer_id, u64 offset, bool use_u16) {
}

auto CommandList::set_viewport(const vk::Viewport &viewport) -> void {
}

auto CommandList::set_scissors(const vk::Rect2D &rect) -> void {
}

auto CommandList::draw(u32 vertex_count, u32 first_vertex, u32 instance_count, u32 first_instance) -> void {
}

auto CommandList::draw_indexed(u32 index_count, u32 first_index, i32 vertex_offset, u32 instance_count, u32 first_instance) -> void {
}

auto CommandList::dispatch(u32 group_x, u32 group_y, u32 group_z) -> void {
}

auto CommandQueue::create(Device_H device, vk::CommandType type) -> std::expected<CommandQueue, vk::Result> {
    ZoneScoped;

    auto impl = new Impl;

    vkb::QueueType vkb_types[] = {
        vkb::QueueType::graphics,  // CommandType::Graphics,
        vkb::QueueType::compute,   // CommandType::Compute,
        vkb::QueueType::transfer,  // CommandType::Transfer,
    };
    vkb::QueueType vkb_type = vkb_types[std::to_underlying(type)];

    auto queue_handle = device->handle.get_queue(vkb_type);
    if (!queue_handle) {
        LOG_ERROR("Failed to create Device Queue! {}", queue_handle.error().message());
        return std::unexpected(vk::Result::InitFailed);
    }

    u32 queue_index = device->handle.get_queue_index(vkb_type).value();

    impl->device = device;
    impl->type = type;
    impl->family_index = queue_index;
    impl->semaphore = Semaphore::create(device, 0).value();
    impl->handle = queue_handle.value();

    return CommandQueue(impl);
}

auto CommandQueue::destroy() -> void {
    ZoneScoped;

    impl->semaphore.destroy();
}

auto CommandQueue::defer(ls::span<BufferID> buffer_ids) -> void {
    for (auto &v : buffer_ids) {
        impl->garbage_buffers.emplace(v, impl->semaphore.counter());
    }
}

auto CommandQueue::defer(ls::span<ImageID> image_ids) -> void {
    for (auto &v : image_ids) {
        impl->garbage_images.emplace(v, impl->semaphore.counter());
    }
}

auto CommandQueue::defer(ls::span<ImageViewID> image_view_ids) -> void {
    for (auto &v : image_view_ids) {
        impl->garbage_image_views.emplace(v, impl->semaphore.counter());
    }
}

auto CommandQueue::defer(ls::span<SamplerID> sampler_ids) -> void {
    for (auto &v : sampler_ids) {
        impl->garbage_samplers.emplace(v, impl->semaphore.counter());
    }
}

auto CommandQueue::begin(std::source_location LOC) -> CommandList {
    ZoneScoped;
    memory::ScopedStack stack;

    auto cmd_list = impl->command_lists.create();

    auto cmd_list_impl = cmd_list->impl;
    cmd_list_impl->device = impl->device;
    cmd_list_impl->id = cmd_list->id;

    if (cmd_list->is_fresh) {
        VkCommandPoolCreateInfo cmd_alloc_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext = nullptr,
            .flags = VkCommandPoolCreateFlags{},
            .queueFamilyIndex = impl->family_index,
        };
        vkCreateCommandPool(impl->device->handle, &cmd_alloc_info, nullptr, &cmd_list_impl->allocator);

        VkCommandBufferAllocateInfo cmd_list_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext = nullptr,
            .commandPool = cmd_list_impl->allocator,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };
        vkAllocateCommandBuffers(impl->device->handle, &cmd_list_info, &cmd_list_impl->handle);
    } else {
        // VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT is like "fully" destroying
        // a std::vector<T> (aka freeing its memory). If this flag is set to 0,
        // its like calling vector.clear(), it doesnt free the memory but sets
        // offset to zero. In our case, it's more efficient to not set this flag
        vkResetCommandPool(impl->device->handle, cmd_list_impl->allocator, 0);
    }

#if LS_DEBUG
    auto cmd_list_name = stack.format("{}:{}", LOC.file_name(), LOC.line());
    set_object_name(impl->device, cmd_list_impl->handle, VK_OBJECT_TYPE_COMMAND_BUFFER, cmd_list_name);
    set_object_name(impl->device, cmd_list_impl->allocator, VK_OBJECT_TYPE_COMMAND_POOL, cmd_list_name);
#endif

    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr,
    };
    vkBeginCommandBuffer(cmd_list_impl->handle, &begin_info);

    return CommandList(cmd_list_impl);
}

auto CommandQueue::end(CommandList &cmd_list) -> void {
    ZoneScoped;

    vkEndCommandBuffer(cmd_list->handle);
    impl->garbage_command_lists.emplace(cmd_list->id, impl->semaphore.counter());
}

auto CommandQueue::submit(ls::span<Semaphore_H> wait_semas) -> std::expected<void, vk::Result> {
}

auto CommandQueue::present(SwapChain_H swap_chain, Semaphore_H present_sema, u32 image_id) -> std::expected<void, vk::Result> {
}

auto CommandQueue::wait() -> void {
}

}  // namespace lr
