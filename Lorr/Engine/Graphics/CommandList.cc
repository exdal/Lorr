#include "CommandList.hh"

#include "Device.hh"

namespace lr::graphics {
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
    return CHECK(vkQueuePresentKHR(m_handle, &present_info));
}

void CommandList::set_pipeline_barrier(DependencyInfo &dependency_info)
{
    ZoneScoped;

    vkCmdPipelineBarrier2(m_handle, reinterpret_cast<const VkDependencyInfo *>(&dependency_info));
}

void CommandList::copy_buffer_to_buffer(Buffer *src, Buffer *dst, std::span<BufferCopyRegion> regions)
{
    ZoneScoped;

    vkCmdCopyBuffer(m_handle, *src, *dst, regions.size(), (VkBufferCopy *)regions.data());
}

void CommandList::copy_buffer_to_image(Buffer *src, Image *dst, ImageLayout layout, std::span<ImageCopyRegion> regions)
{
    ZoneScoped;

    vkCmdCopyBufferToImage(
        m_handle, src->m_handle, dst->m_handle, static_cast<VkImageLayout>(layout), regions.size(), (VkBufferImageCopy *)regions.data());
}

void CommandList::begin_rendering(const RenderingBeginInfo &info)
{
    ZoneScoped;

    vkCmdBeginRendering(m_handle, (VkRenderingInfo *)&info);
}

void CommandList::end_rendering()
{
    ZoneScoped;

    vkCmdEndRendering(m_handle);
}

void CommandList::set_pipeline(Pipeline *pipeline)
{
    ZoneScoped;

    vkCmdBindPipeline(m_handle, static_cast<VkPipelineBindPoint>(pipeline->m_bind_point), pipeline->m_handle);
}

void CommandList::set_push_constants(PipelineLayout *layout, void *data, u32 data_size, u32 offset, ShaderStageFlag stage_flags)
{
    ZoneScoped;

    vkCmdPushConstants(m_handle, layout->m_handle, static_cast<VkShaderStageFlags>(stage_flags), offset, data_size, data);
}

void CommandList::set_descriptor_sets(
    PipelineLayout *layout, PipelineBindPoint bind_point, u32 first_set, const ls::static_vector<VkDescriptorSet, 16> &sets)
{
    ZoneScoped;

    vkCmdBindDescriptorSets(
        m_handle, static_cast<VkPipelineBindPoint>(bind_point), layout->m_handle, first_set, sets.size(), sets.data(), 0, nullptr);
}

void CommandList::set_vertex_buffer(Buffer *buffer, u64 offset, u32 first_binding, u32 binding_count)
{
    ZoneScoped;

    vkCmdBindVertexBuffers(m_handle, first_binding, binding_count, &buffer->m_handle, &offset);
}

void CommandList::set_index_buffer(Buffer *buffer, u64 offset, bool use_u16)
{
    ZoneScoped;

    vkCmdBindIndexBuffer(m_handle, buffer->m_handle, offset, use_u16 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32);
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

CommandBatcher::CommandBatcher(Device *device, CommandList &command_list)
    : m_device(device),
      m_command_list(command_list)
{
}

CommandBatcher::CommandBatcher(Unique<CommandList> &command_list)
    : m_device(command_list.m_device),
      m_command_list(command_list.m_val)
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
    raw_barrier.image = *m_device->get_image(image_id);
}

void CommandBatcher::flush_barriers()
{
    if (m_image_barriers.empty() && m_memory_barriers.empty()) {
        return;
    }

    DependencyInfo dependency_info = {
        .memory_barrier_count = static_cast<u32>(m_memory_barriers.size()),
        .memory_barriers = m_memory_barriers.data(),
        .image_barrier_count = static_cast<u32>(m_image_barriers.size()),
        .image_barriers = m_image_barriers.data(),
    };

    m_command_list.set_pipeline_barrier(dependency_info);

    m_image_barriers.clear();
    m_memory_barriers.clear();
}

}  // namespace lr::graphics
