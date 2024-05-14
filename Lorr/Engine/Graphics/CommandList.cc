#include "CommandList.hh"

#include "Device.hh"

#include "Memory/Stack.hh"

namespace lr::graphics {
void CommandQueue::defer(std::span<const BufferID> buffer_ids)
{
    ZoneScoped;

    for (BufferID v : buffer_ids) {
        m_garbage_buffers.emplace(v, m_semaphore.counter());
    }
}

void CommandQueue::defer(std::span<const ImageID> image_ids)
{
    ZoneScoped;

    for (ImageID v : image_ids) {
        m_garbage_images.emplace(v, m_semaphore.counter());
    }
}

void CommandQueue::defer(std::span<const ImageViewID> image_view_ids)
{
    ZoneScoped;

    for (ImageViewID v : image_view_ids) {
        m_garbage_image_views.emplace(v, m_semaphore.counter());
    }
}

void CommandQueue::defer(std::span<const SamplerID> sampler_ids)
{
    ZoneScoped;

    for (SamplerID v : sampler_ids) {
        m_garbage_samplers.emplace(v, m_semaphore.counter());
    }
}

VKResult CommandQueue::submit(QueueSubmitInfo &submit_info)
{
    ZoneScoped;

    return CHECK(vkQueueSubmit2(m_handle, 1, reinterpret_cast<VkSubmitInfo2 *>(&submit_info), nullptr));
}

VKResult CommandQueue::present(SwapChain &swap_chain, Semaphore &present_sema, u32 image_id)
{
    ZoneScoped;

    VkPresentInfoKHR present_info = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext = nullptr,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &present_sema.m_handle,
        .swapchainCount = 1,
        .pSwapchains = &swap_chain.m_handle.swapchain,
        .pImageIndices = &image_id,
        .pResults = nullptr,
    };
    return static_cast<VKResult>(vkQueuePresentKHR(m_handle, &present_info));
}

void CommandList::image_transition(const ImageBarrier &barrier)
{
    ZoneScoped;

    Image *image = m_device->get_image(barrier.image_id);
    VkImageMemoryBarrier2 vk_barrier = static_cast<ImageBarrier>(barrier).vk_type(*image);

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
    vkCmdPipelineBarrier2(m_handle, &dependency_info);
}

void CommandList::memory_barrier(const MemoryBarrier &barrier)
{
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
    vkCmdPipelineBarrier2(m_handle, &dependency_info);
}

void CommandList::set_barriers(std::span<const MemoryBarrier> memory, std::span<const ImageBarrier> image)
{
    ZoneScoped;
    memory::ScopedStack stack;

    auto memory_barriers = stack.alloc<VkMemoryBarrier2>(memory.size());
    auto image_barriers = stack.alloc<VkImageMemoryBarrier2>(image.size());

    for (u32 i = 0; i < memory.size(); i++) {
        memory_barriers[i] = static_cast<MemoryBarrier>(memory[i]).vk_type();
    }

    for (u32 i = 0; i < image.size(); i++) {
        image_barriers[i] = static_cast<ImageBarrier>(image[i]).vk_type(*m_device->get_image(image[i].image_id));
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
    vkCmdPipelineBarrier2(m_handle, &dependency_info);
}

void CommandList::copy_buffer_to_buffer(BufferID src, BufferID dst, std::span<const BufferCopyRegion> regions)
{
    ZoneScoped;

    vkCmdCopyBuffer(m_handle, *m_device->get_buffer(src), *m_device->get_buffer(dst), regions.size(), (VkBufferCopy *)regions.data());
}

void CommandList::copy_buffer_to_image(BufferID src, ImageID dst, ImageLayout layout, std::span<const ImageCopyRegion> regions)
{
    ZoneScoped;

    vkCmdCopyBufferToImage(
        m_handle, *m_device->get_buffer(src), *m_device->get_image(dst), static_cast<VkImageLayout>(layout), regions.size(), (VkBufferImageCopy *)regions.data());
}

void CommandList::blit_image(ImageID src, ImageLayout src_layout, ImageID dst, ImageLayout dst_layout, Filtering filter, std::span<const ImageBlit> blits)
{
    ZoneScoped;

    VkBlitImageInfo2 blit_info = {
        .sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
        .pNext = nullptr,
        .srcImage = *m_device->get_image(src),
        .srcImageLayout = static_cast<VkImageLayout>(src_layout),
        .dstImage = *m_device->get_image(dst),
        .dstImageLayout = static_cast<VkImageLayout>(dst_layout),
        .regionCount = static_cast<u32>(blits.size()),
        .pRegions = reinterpret_cast<const VkImageBlit2 *>(blits.data()),
        .filter = static_cast<VkFilter>(filter),
    };
    vkCmdBlitImage2(m_handle, &blit_info);
}

void CommandList::begin_rendering(const RenderingBeginInfo &info)
{
    ZoneScoped;
    memory::ScopedStack stack;

    auto color_attachments = stack.alloc<VkRenderingAttachmentInfo>(info.color_attachments.size());
    VkRenderingAttachmentInfo depth_attachment = {};
    VkRenderingAttachmentInfo stencil_attachment = {};

    for (u32 i = 0; i < info.color_attachments.size(); i++) {
        ImageView *image_view = m_device->get_image_view(info.color_attachments[i].image_view_id);
        color_attachments[i] = info.color_attachments[i].vk_info(*image_view);
    }

    if (info.depth_attachment) {
        ImageView *image_view = m_device->get_image_view(info.depth_attachment->image_view_id);
        depth_attachment = info.depth_attachment->vk_info(*image_view);
    }

    if (info.stencil_attachment) {
        ImageView *image_view = m_device->get_image_view(info.stencil_attachment->image_view_id);
        stencil_attachment = info.stencil_attachment->vk_info(*image_view);
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
    vkCmdBeginRendering(m_handle, &rendering_info);
}

void CommandList::end_rendering()
{
    ZoneScoped;

    vkCmdEndRendering(m_handle);
}

void CommandList::set_pipeline(PipelineID pipeline_id)
{
    ZoneScoped;

    Pipeline *pipeline = m_device->get_pipeline(pipeline_id);
    vkCmdBindPipeline(m_handle, static_cast<VkPipelineBindPoint>(pipeline->m_bind_point), pipeline->m_handle);
}

void CommandList::set_push_constants(PipelineLayout &layout, void *data, u32 data_size, u32 offset, ShaderStageFlag stage_flags)
{
    ZoneScoped;

    vkCmdPushConstants(m_handle, layout, static_cast<VkShaderStageFlags>(stage_flags), offset, data_size, data);
}

void CommandList::set_descriptor_sets(PipelineLayout &layout, PipelineBindPoint bind_point, u32 first_set, std::span<DescriptorSet> sets)
{
    ZoneScoped;
    memory::ScopedStack stack;

    auto descriptor_sets = stack.alloc<VkDescriptorSet>(sets.size());
    for (u32 i = 0; i < sets.size(); i++) {
        descriptor_sets[i] = sets[i];
    }

    vkCmdBindDescriptorSets(m_handle, static_cast<VkPipelineBindPoint>(bind_point), layout, first_set, sets.size(), descriptor_sets.data(), 0, nullptr);
}

void CommandList::set_vertex_buffer(BufferID buffer_id, u64 offset, u32 first_binding, u32 binding_count)
{
    ZoneScoped;

    Buffer *buffer = m_device->get_buffer(buffer_id);
    vkCmdBindVertexBuffers(m_handle, first_binding, binding_count, &buffer->m_handle, &offset);
}

void CommandList::set_index_buffer(BufferID buffer_id, u64 offset, bool use_u16)
{
    ZoneScoped;

    Buffer *buffer = m_device->get_buffer(buffer_id);
    vkCmdBindIndexBuffer(m_handle, *buffer, offset, use_u16 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32);
}

void CommandList::set_viewport(u32 id, const Viewport &viewport)
{
    ZoneScoped;

    vkCmdSetViewport(m_handle, id, 1, (VkViewport *)&viewport);
}

void CommandList::set_scissors(u32 id, const Rect2D &rect)
{
    ZoneScoped;

    vkCmdSetScissor(m_handle, id, 1, (VkRect2D *)&rect);
}

void CommandList::draw(u32 vertex_count, u32 first_vertex, u32 instance_count, u32 first_instance)
{
    ZoneScoped;

    vkCmdDraw(m_handle, vertex_count, instance_count, first_vertex, first_instance);
}

void CommandList::draw_indexed(u32 index_count, u32 first_index, i32 vertex_offset, u32 instance_count, u32 first_instance)
{
    ZoneScoped;

    vkCmdDrawIndexed(m_handle, index_count, instance_count, first_index, vertex_offset, first_instance);
}

void CommandList::dispatch(u32 group_x, u32 group_y, u32 group_z)
{
    ZoneScoped;

    vkCmdDispatch(m_handle, group_x, group_y, group_z);
}

CommandBatcher::CommandBatcher(CommandList &command_list)
    : m_command_list(command_list)
{
}

CommandBatcher::CommandBatcher(Unique<CommandList> &command_list)
    : m_command_list(command_list.m_val)
{
}

CommandBatcher::~CommandBatcher()
{
    flush_barriers();
}

void CommandBatcher::insert_memory_barrier(const MemoryBarrier &barrier)
{
    if (m_memory_barriers.full()) {
        flush_barriers();
    }

    m_memory_barriers.push_back(barrier);
}

void CommandBatcher::insert_image_barrier(const ImageBarrier &barrier)
{
    if (m_image_barriers.full()) {
        flush_barriers();
    }

    ImageID image_id = barrier.image_id;
    ImageBarrier &raw_barrier = m_image_barriers.emplace_back(barrier);
}

void CommandBatcher::flush_barriers()
{
    if (m_image_barriers.empty() && m_memory_barriers.empty()) {
        return;
    }

    m_command_list.set_barriers(m_memory_barriers, m_image_barriers);
    m_image_barriers.clear();
    m_memory_barriers.clear();
}

}  // namespace lr::graphics
