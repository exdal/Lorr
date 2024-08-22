#include "CommandList.hh"

#include "Device.hh"

#include "Engine/Memory/Stack.hh"

namespace lr {
void CommandList::reset_query_pool(this CommandList &self, TimestampQueryPool &query_pool, u32 first_query, u32 query_count) {
    ZoneScoped;

    vkCmdResetQueryPool(self, query_pool, first_query, query_count);
}

void CommandList::write_timestamp(this CommandList &self, TimestampQueryPool &query_pool, PipelineStage pipeline_stage, u32 query_index) {
    ZoneScoped;

    vkCmdWriteTimestamp2(self, static_cast<VkPipelineStageFlags2>(pipeline_stage), query_pool, query_index);
}

void CommandList::image_transition(this CommandList &self, const ImageBarrier &barrier, std::source_location LOC) {
    ZoneScoped;

    Image &image = self.device->image_at(barrier.image_id);
    VkImageMemoryBarrier2 vk_barrier = static_cast<ImageBarrier>(barrier).vk_type(image);

    VkDependencyInfo dependency_info = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .pNext = nullptr,
        .dependencyFlags = 0,
        .memoryBarrierCount = 0,
        .pMemoryBarriers = nullptr,
        .bufferMemoryBarrierCount = 0,
        .pBufferMemoryBarriers = nullptr,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &vk_barrier,
    };
    vkCmdPipelineBarrier2(self, &dependency_info);
}

void CommandList::memory_barrier(this CommandList &self, const MemoryBarrier &barrier, std::source_location LOC) {
    ZoneScoped;

    VkMemoryBarrier2 vk_barrier = static_cast<MemoryBarrier>(barrier).vk_type();

    VkDependencyInfo dependency_info = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .pNext = nullptr,
        .dependencyFlags = 0,
        .memoryBarrierCount = 1,
        .pMemoryBarriers = &vk_barrier,
        .bufferMemoryBarrierCount = 0,
        .pBufferMemoryBarriers = nullptr,
        .imageMemoryBarrierCount = 0,
        .pImageMemoryBarriers = nullptr,
    };
    vkCmdPipelineBarrier2(self, &dependency_info);
}

void CommandList::set_barriers(this CommandList &self, ls::span<MemoryBarrier> memory, ls::span<ImageBarrier> image, std::source_location LOC) {
    ZoneScoped;
    memory::ScopedStack stack;

    auto memory_barriers = stack.alloc<VkMemoryBarrier2>(memory.size());
    auto image_barriers = stack.alloc<VkImageMemoryBarrier2>(image.size());

    for (u32 i = 0; i < memory.size(); i++) {
        memory_barriers[i] = static_cast<MemoryBarrier>(memory[i]).vk_type();
    }

    for (u32 i = 0; i < image.size(); i++) {
        image_barriers[i] = static_cast<ImageBarrier>(image[i]).vk_type(self.device->image_at(image[i].image_id));
    }

    VkDependencyInfo dependency_info = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .pNext = nullptr,
        .dependencyFlags = 0,
        .memoryBarrierCount = static_cast<u32>(memory_barriers.size()),
        .pMemoryBarriers = memory_barriers.data(),
        .bufferMemoryBarrierCount = 0,
        .pBufferMemoryBarriers = nullptr,
        .imageMemoryBarrierCount = static_cast<u32>(image_barriers.size()),
        .pImageMemoryBarriers = image_barriers.data(),
    };
    vkCmdPipelineBarrier2(self, &dependency_info);
}

void CommandList::copy_buffer_to_buffer(this CommandList &self, BufferID src, BufferID dst, ls::span<BufferCopyRegion> regions) {
    ZoneScoped;

    vkCmdCopyBuffer(self, self.device->buffer_at(src), self.device->buffer_at(dst), regions.size(), (VkBufferCopy *)regions.data());
}

void CommandList::copy_buffer_to_image(this CommandList &self, BufferID src, ImageID dst, ImageLayout layout, ls::span<ImageCopyRegion> regions) {
    ZoneScoped;

    vkCmdCopyBufferToImage(
        self,
        self.device->buffer_at(src),
        self.device->image_at(dst),
        static_cast<VkImageLayout>(layout),
        regions.size(),
        reinterpret_cast<VkBufferImageCopy *>(regions.data()));
}

void CommandList::blit_image(
    this CommandList &self, ImageID src, ImageLayout src_layout, ImageID dst, ImageLayout dst_layout, Filtering filter, ls::span<ImageBlit> blits) {
    ZoneScoped;

    VkBlitImageInfo2 blit_info = {
        .sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
        .pNext = nullptr,
        .srcImage = self.device->image_at(src),
        .srcImageLayout = static_cast<VkImageLayout>(src_layout),
        .dstImage = self.device->image_at(dst),
        .dstImageLayout = static_cast<VkImageLayout>(dst_layout),
        .regionCount = static_cast<u32>(blits.size()),
        .pRegions = reinterpret_cast<const VkImageBlit2 *>(blits.data()),
        .filter = static_cast<VkFilter>(filter),
    };
    vkCmdBlitImage2(self, &blit_info);
}

void CommandList::begin_rendering(this CommandList &self, const RenderingBeginInfo &info) {
    ZoneScoped;
    memory::ScopedStack stack;

    auto color_attachments = stack.alloc<VkRenderingAttachmentInfo>(info.color_attachments.size());
    VkRenderingAttachmentInfo depth_attachment = {};
    VkRenderingAttachmentInfo stencil_attachment = {};

    for (u32 i = 0; i < info.color_attachments.size(); i++) {
        ImageView &image_view = self.device->image_view_at(info.color_attachments[i].image_view_id);
        color_attachments[i] = info.color_attachments[i].vk_info(image_view);
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
    vkCmdBeginRendering(self, &rendering_info);
}

void CommandList::end_rendering(this CommandList &self) {
    ZoneScoped;

    vkCmdEndRendering(self);
}

void CommandList::set_pipeline(this CommandList &self, PipelineID pipeline_id) {
    ZoneScoped;

    Pipeline &pipeline = self.device->pipeline_at(pipeline_id);
    vkCmdBindPipeline(self, static_cast<VkPipelineBindPoint>(pipeline.bind_point), pipeline);
}

void CommandList::set_push_constants(this CommandList &self, PipelineLayoutID layout_id, void *data, u32 data_size, u32 offset, ShaderStageFlag stage_flags) {
    ZoneScoped;

    PipelineLayout &layout = self.device->pipeline_layout_at(layout_id);
    vkCmdPushConstants(self, layout, static_cast<VkShaderStageFlags>(stage_flags), offset, data_size, data);
}

void CommandList::set_descriptor_sets(this CommandList &self, PipelineLayoutID layout_id, PipelineBindPoint bind_point, u32 first_set, ls::span<DescriptorSet> sets) {
    ZoneScoped;
    memory::ScopedStack stack;

    auto descriptor_sets = stack.alloc<VkDescriptorSet>(sets.size());
    for (u32 i = 0; i < sets.size(); i++) {
        descriptor_sets[i] = sets[i];
    }

    PipelineLayout &layout = self.device->pipeline_layout_at(layout_id);
    vkCmdBindDescriptorSets(self, static_cast<VkPipelineBindPoint>(bind_point), layout, first_set, sets.size(), descriptor_sets.data(), 0, nullptr);
}

void CommandList::set_vertex_buffer(this CommandList &self, BufferID buffer_id, u64 offset, u32 first_binding, u32 binding_count) {
    ZoneScoped;

    Buffer &buffer = self.device->buffer_at(buffer_id);
    vkCmdBindVertexBuffers(self, first_binding, binding_count, &buffer.handle, &offset);
}

void CommandList::set_index_buffer(this CommandList &self, BufferID buffer_id, u64 offset, bool use_u16) {
    ZoneScoped;

    Buffer &buffer = self.device->buffer_at(buffer_id);
    vkCmdBindIndexBuffer(self, buffer, offset, use_u16 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32);
}

void CommandList::set_viewport(this CommandList &self, u32 id, const Viewport &viewport) {
    ZoneScoped;

    vkCmdSetViewport(self, id, 1, reinterpret_cast<const VkViewport *>(&viewport));
}

void CommandList::set_scissors(this CommandList &self, u32 id, const Rect2D &rect) {
    ZoneScoped;

    vkCmdSetScissor(self, id, 1, reinterpret_cast<const VkRect2D *>(&rect));
}

void CommandList::draw(this CommandList &self, u32 vertex_count, u32 first_vertex, u32 instance_count, u32 first_instance) {
    ZoneScoped;

    vkCmdDraw(self, vertex_count, instance_count, first_vertex, first_instance);
}

void CommandList::draw_indexed(this CommandList &self, u32 index_count, u32 first_index, i32 vertex_offset, u32 instance_count, u32 first_instance) {
    ZoneScoped;

    vkCmdDrawIndexed(self, index_count, instance_count, first_index, vertex_offset, first_instance);
}

void CommandList::dispatch(this CommandList &self, u32 group_x, u32 group_y, u32 group_z) {
    ZoneScoped;

    vkCmdDispatch(self, group_x, group_y, group_z);
}

CommandBatcher::CommandBatcher(CommandList &command_list)
    : command_list(command_list) {
}

CommandBatcher::~CommandBatcher() {
    ZoneScoped;

    flush_barriers();
}

void CommandBatcher::insert_memory_barrier(const MemoryBarrier &barrier) {
    ZoneScoped;

    if (m_memory_barriers.full()) {
        flush_barriers();
    }

    m_memory_barriers.push_back(barrier);
}

void CommandBatcher::insert_image_barrier(const ImageBarrier &barrier) {
    ZoneScoped;

    if (m_image_barriers.full()) {
        flush_barriers();
    }

    m_image_barriers.emplace_back(barrier);
}

void CommandBatcher::flush_barriers() {
    ZoneScoped;

    if (m_image_barriers.empty() && m_memory_barriers.empty()) {
        return;
    }

    command_list.set_barriers(m_memory_barriers, m_image_barriers);
    m_image_barriers.clear();
    m_memory_barriers.clear();
}

void CommandQueue::defer(this CommandQueue &self, ls::span<BufferID> buffer_ids) {
    ZoneScoped;

    for (BufferID v : buffer_ids) {
        self.garbage_buffers.emplace(v, self.semaphore.counter);
    }
}

void CommandQueue::defer(this CommandQueue &self, ls::span<ImageID> image_ids) {
    ZoneScoped;

    for (ImageID v : image_ids) {
        self.garbage_images.emplace(v, self.semaphore.counter);
    }
}

void CommandQueue::defer(this CommandQueue &self, ls::span<ImageViewID> image_view_ids) {
    ZoneScoped;

    for (ImageViewID v : image_view_ids) {
        self.garbage_image_views.emplace(v, self.semaphore.counter);
    }
}

void CommandQueue::defer(this CommandQueue &self, ls::span<SamplerID> sampler_ids) {
    ZoneScoped;

    for (SamplerID v : sampler_ids) {
        self.garbage_samplers.emplace(v, self.semaphore.counter);
    }
}

void CommandQueue::defer(this CommandQueue &self, ls::span<CommandList> command_lists) {
    ZoneScoped;

    for (const CommandList &command_list : command_lists) {
        self.garbage_command_lists.emplace(command_list, self.semaphore.counter);
    }
}

CommandList &CommandQueue::begin_command_list(this CommandQueue &self, usize frame_index, std::source_location LOC) {
    ZoneScoped;
    memory::ScopedStack stack;

    auto &v = self.command_lists[frame_index];
    auto it = v.emplace();
    CommandAllocator &cmd_allocator = self.allocators[frame_index];
    CommandList &cmd_list = *it;

    self.device->create_command_lists(cmd_list, cmd_allocator);
    self.device->set_object_name(cmd_list, stack.format("At {}:{} {}", LOC.file_name(), LOC.line(), LOC.function_name()));
    cmd_list.rendering_frame = frame_index;

    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr,
    };
    vkBeginCommandBuffer(cmd_list, &begin_info);

    return cmd_list;
}

void CommandQueue::end_command_list(this CommandQueue &self, CommandList &cmd_list) {
    ZoneScoped;

    auto &frame_lists = self.frame_cmd_submits[cmd_list.rendering_frame];
    frame_lists.emplace_back(cmd_list);

    vkEndCommandBuffer(cmd_list);

    self.defer(cmd_list);
}

VKResult CommandQueue::submit(this CommandQueue &self, usize frame_index, const QueueSubmitInfo &info) {
    ZoneScoped;
    memory::ScopedStack stack;

    auto &cmd_lists = self.command_lists[frame_index];
    auto &cmd_submits = self.frame_cmd_submits[frame_index];

    SemaphoreSubmitInfo self_waits[] = { { self.semaphore, self.semaphore.counter, PipelineStage::AllCommands } };
    SemaphoreSubmitInfo self_signals[] = { { self.semaphore, self.semaphore.advance(), PipelineStage::AllCommands } };

    u32 all_waits_size = 0;
    auto all_waits = stack.alloc<SemaphoreSubmitInfo>(info.additional_wait_semas.size() + count_of(self_waits));
    for (const SemaphoreSubmitInfo &v : info.additional_wait_semas)
        all_waits[all_waits_size++] = v;

    if (info.self_wait) {
        for (const SemaphoreSubmitInfo &v : self_waits)
            all_waits[all_waits_size++] = v;
    }

    u32 all_signals_size = 0;
    auto all_signals = stack.alloc<SemaphoreSubmitInfo>(info.additional_signal_semas.size() + count_of(self_signals));
    for (const SemaphoreSubmitInfo &v : info.additional_signal_semas)
        all_signals[all_signals_size++] = v;
    for (const SemaphoreSubmitInfo &v : self_signals)
        all_signals[all_signals_size++] = v;

    VkSubmitInfo2 submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .pNext = nullptr,
        .flags = 0,
        .waitSemaphoreInfoCount = all_waits_size,
        .pWaitSemaphoreInfos = reinterpret_cast<const VkSemaphoreSubmitInfo *>(all_waits.data()),
        .commandBufferInfoCount = static_cast<u32>(cmd_submits.size()),
        .pCommandBufferInfos = reinterpret_cast<VkCommandBufferSubmitInfo *>(cmd_submits.data()),
        .signalSemaphoreInfoCount = all_signals_size,
        .pSignalSemaphoreInfos = reinterpret_cast<const VkSemaphoreSubmitInfo *>(all_signals.data()),
    };
    auto result = static_cast<VKResult>(vkQueueSubmit2(self, 1, &submit_info, nullptr));

    // POST CLEANUP //
    cmd_lists.clear();
    cmd_submits.clear();

    return result;
}

VKResult CommandQueue::present(this CommandQueue &self, SwapChain &swap_chain, Semaphore &present_sema, u32 image_id) {
    ZoneScoped;

    VkPresentInfoKHR present_info = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext = nullptr,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &present_sema.handle,
        .swapchainCount = 1,
        .pSwapchains = &swap_chain.handle.swapchain,
        .pImageIndices = &image_id,
        .pResults = nullptr,
    };
    return static_cast<VKResult>(vkQueuePresentKHR(self, &present_info));
}

void CommandQueue::wait_for_work(this CommandQueue &self) {
    ZoneScoped;

    vkQueueWaitIdle(self);
}

}  // namespace lr
