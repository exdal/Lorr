#include "CommandList.hh"

namespace lr::Graphics {
static constexpr VkAttachmentLoadOp kLoadOpLUT[] = {
    VK_ATTACHMENT_LOAD_OP_DONT_CARE,  // DontCare
    VK_ATTACHMENT_LOAD_OP_LOAD,       // Load
    VK_ATTACHMENT_LOAD_OP_MAX_ENUM,   // Store
    VK_ATTACHMENT_LOAD_OP_CLEAR,      // Clear
};
static constexpr VkAttachmentStoreOp kStoreOpLUT[] = {
    VK_ATTACHMENT_STORE_OP_DONT_CARE,  // DontCare
    VK_ATTACHMENT_STORE_OP_MAX_ENUM,   // Load
    VK_ATTACHMENT_STORE_OP_STORE,      // Store
    VK_ATTACHMENT_STORE_OP_MAX_ENUM,   // Clear
};

DescriptorBufferBindInfo::DescriptorBufferBindInfo(Buffer *buffer, BufferUsage buffer_usage)
    : VkDescriptorBufferBindingInfoEXT(VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT)
{
    this->address = buffer->m_device_address;
    this->usage = static_cast<VkBufferUsageFlags>(buffer_usage);
}

RenderingAttachment::RenderingAttachment(ImageView *image_view, ImageLayout layout, AttachmentOp load_op, AttachmentOp store_op)
    : VkRenderingAttachmentInfo(VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO)
{
    this->imageView = image_view->m_handle;
    this->imageLayout = static_cast<VkImageLayout>(layout);
    this->resolveMode = VK_RESOLVE_MODE_NONE;
    this->resolveImageView = nullptr;
    this->resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    this->loadOp = kLoadOpLUT[(u32)load_op];
    this->storeOp = kStoreOpLUT[(u32)store_op];
}

RenderingAttachment::RenderingAttachment(
    ImageView *image_view, ImageLayout layout, AttachmentOp load_op, AttachmentOp store_op, ColorClearValue clear_val)
    : RenderingAttachment(image_view, layout, load_op, store_op)
{
    memcpy(&this->clearValue.color, &clear_val, sizeof(ColorClearValue));
}

RenderingAttachment::RenderingAttachment(
    ImageView *image_view, ImageLayout layout, AttachmentOp load_op, AttachmentOp store_op, DepthClearValue clear_val)
    : RenderingAttachment(image_view, layout, load_op, store_op)
{
    memcpy(&this->clearValue.color, &clear_val, sizeof(DepthClearValue));
}

ImageBarrier::ImageBarrier(Image *image, ImageSubresourceInfo slice_info, const PipelineBarrier &barrier)
    : VkImageMemoryBarrier2(VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2)
{
    ZoneScoped;

    this->srcStageMask = static_cast<VkPipelineStageFlags2>(barrier.m_src_stage);
    this->srcAccessMask = static_cast<VkAccessFlags2>(barrier.m_src_access);
    this->dstStageMask = static_cast<VkPipelineStageFlags2>(barrier.m_dst_stage);
    this->dstAccessMask = static_cast<VkAccessFlags2>(barrier.m_dst_access);
    this->oldLayout = static_cast<VkImageLayout>(barrier.m_src_layout);
    this->newLayout = static_cast<VkImageLayout>(barrier.m_dst_layout);
    this->srcQueueFamilyIndex = barrier.m_src_queue_index;
    this->dstQueueFamilyIndex = barrier.m_dst_queue_index;
    this->image = image->m_handle;
    this->subresourceRange = ImageSubresourceRange(slice_info);
}

MemoryBarrier::MemoryBarrier(const PipelineBarrier &barrier)
    : VkMemoryBarrier2(VK_STRUCTURE_TYPE_MEMORY_BARRIER_2)
{
    ZoneScoped;

    this->srcStageMask = static_cast<VkPipelineStageFlags2>(barrier.m_src_stage);
    this->dstStageMask = static_cast<VkPipelineStageFlags2>(barrier.m_dst_stage);
    this->srcAccessMask = static_cast<VkAccessFlags2>(barrier.m_src_access);
    this->dstAccessMask = static_cast<VkAccessFlags2>(barrier.m_dst_access);
}

DependencyInfo::DependencyInfo()
    : VkDependencyInfo(VK_STRUCTURE_TYPE_DEPENDENCY_INFO)
{
}

DependencyInfo::DependencyInfo(eastl::span<ImageBarrier> image_barriers, eastl::span<MemoryBarrier> memory_barriers)
    : DependencyInfo()
{
    ZoneScoped;

    this->imageMemoryBarrierCount = image_barriers.size();
    this->pImageMemoryBarriers = image_barriers.data();
    this->memoryBarrierCount = memory_barriers.size();
    this->pMemoryBarriers = memory_barriers.data();
}

SemaphoreSubmitDesc::SemaphoreSubmitDesc(Semaphore *semaphore, PipelineStage stage)
    : VkSemaphoreSubmitInfo()
{
    this->sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    this->pNext = nullptr;
    this->semaphore = semaphore->m_handle;
    this->value = 0;
    this->stageMask = static_cast<VkPipelineStageFlags2>(stage);
    this->deviceIndex = 0;
}

SemaphoreSubmitDesc::SemaphoreSubmitDesc(Semaphore *semaphore, u64 value, PipelineStage stage)
    : VkSemaphoreSubmitInfo()
{
    this->sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    this->pNext = nullptr;
    this->semaphore = semaphore->m_handle;
    this->value = value;
    this->stageMask = static_cast<VkPipelineStageFlags2>(stage);
    this->deviceIndex = 0;
}

SemaphoreSubmitDesc::SemaphoreSubmitDesc(Semaphore *semaphore, u64 value)
    : VkSemaphoreSubmitInfo()
{
    this->sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    this->pNext = nullptr;
    this->semaphore = semaphore->m_handle;
    this->value = value;
    this->stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    this->deviceIndex = 0;
}

BufferCopyRegion::BufferCopyRegion(u64 src_offset, u64 dst_offset, u64 size)
    : VkBufferCopy()
{
    this->srcOffset = src_offset;
    this->dstOffset = dst_offset;
    this->size = size;
}

ImageCopyRegion::ImageCopyRegion(const VkBufferImageCopy &lazy)
    : VkBufferImageCopy(lazy)
{
    *this = lazy;
}

ImageCopyRegion::ImageCopyRegion(ImageSubresourceInfo slice_info, u32 width, u32 height, u64 buffer_offset)
    : ImageCopyRegion({})
{
    VkImageSubresourceLayers layer_info = {
        .aspectMask = static_cast<VkImageAspectFlags>(slice_info.m_aspect_mask),
        .mipLevel = slice_info.m_base_mip,
        .baseArrayLayer = slice_info.m_base_slice,
        .layerCount = slice_info.m_slice_count,
    };

    this->bufferOffset = buffer_offset;
    this->imageSubresource = layer_info;
    this->imageExtent.width = width;
    this->imageExtent.height = height;
    this->imageExtent.depth = 1;
}

void CommandList::begin_rendering(const RenderingBeginDesc &desc)
{
    ZoneScoped;

    VkRenderingInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .pNext = nullptr,
        .layerCount = 1,
        .viewMask = 0,  // TODO: Multiviews
        .colorAttachmentCount = static_cast<u32>(desc.m_color_attachments.size()),
        .pColorAttachments = desc.m_color_attachments.data(),
        .pDepthAttachment = desc.m_depth_attachment,
    };
    beginInfo.renderArea.offset.x = static_cast<i32>(desc.m_render_area.x);
    beginInfo.renderArea.offset.y = static_cast<i32>(desc.m_render_area.y);
    beginInfo.renderArea.extent.width = desc.m_render_area.z;
    beginInfo.renderArea.extent.height = desc.m_render_area.w;

    vkCmdBeginRendering(m_handle, &beginInfo);
}

void CommandList::end_rendering()
{
    ZoneScoped;

    vkCmdEndRendering(m_handle);
}

void CommandList::set_pipeline_barrier(DependencyInfo *dependency_info)
{
    ZoneScoped;

    vkCmdPipelineBarrier2(m_handle, dependency_info);
}

void CommandList::copy_buffer_to_buffer(Buffer *src, Buffer *dst, eastl::span<BufferCopyRegion> regions)
{
    ZoneScoped;

    vkCmdCopyBuffer(m_handle, *src, *dst, regions.size(), regions.data());
}

void CommandList::copy_buffer_to_image(Buffer *src, Image *dst, ImageLayout layout, eastl::span<ImageCopyRegion> regions)
{
    ZoneScoped;

    vkCmdCopyBufferToImage(m_handle, src->m_handle, dst->m_handle, static_cast<VkImageLayout>(layout), regions.size(), regions.data());
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

void CommandList::set_viewport(u32 id, const glm::vec2 &pos, const glm::vec2 &size, const glm::vec2 &depth)
{
    ZoneScoped;

    VkViewport vp = {
        .x = pos.x,
        .y = pos.y,
        .width = size.x,
        .height = size.y,
        .minDepth = depth.x,
        .maxDepth = depth.y,
    };
    vkCmdSetViewport(m_handle, id, 1, &vp);
}

void CommandList::set_scissors(u32 id, const glm::ivec2 &pos, const glm::uvec2 &size)
{
    ZoneScoped;

    VkRect2D rect = {
        .offset.x = pos.x,
        .offset.y = pos.y,
        .extent.width = size.x,
        .extent.height = size.y,
    };
    vkCmdSetScissor(m_handle, id, 1, &rect);
}

void CommandList::set_primitive_type(PrimitiveType type)
{
    ZoneScoped;

    vkCmdSetPrimitiveTopology(m_handle, static_cast<VkPrimitiveTopology>(type));
}

void CommandList::set_vertex_buffer(Buffer *buffer, u64 offset, u32 first_binding, u32 binding_count)
{
    ZoneScoped;

    vkCmdBindVertexBuffers(m_handle, first_binding, binding_count, &buffer->m_handle, &offset);
}

void CommandList::set_index_buffer(Buffer *buffer, u64 offset, bool use_u16)
{
    ZoneScoped;

    vkCmdBindIndexBuffer(m_handle, *buffer, offset, use_u16 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32);
}

void CommandList::set_pipeline(Pipeline *pipeline)
{
    ZoneScoped;

    vkCmdBindPipeline(m_handle, static_cast<VkPipelineBindPoint>(pipeline->m_bind_point), pipeline->m_handle);
}

void CommandList::set_push_constants(void *data, u32 data_size, u32 offset, PipelineLayout *layout, ShaderStage stage_flags)
{
    ZoneScoped;

    vkCmdPushConstants(m_handle, layout->m_handle, static_cast<VkShaderStageFlags>(stage_flags), offset, data_size, data);
}

void CommandList::set_descriptor_sets(PipelineBindPoint bind_point, PipelineLayout *layout, u32 first_set, eastl::span<DescriptorSet> sets)
{
    ZoneScoped;

    fixed_vector<VkDescriptorSet, 16> handles = {};
    for (DescriptorSet &set : sets)
        handles.push_back(set.m_handle);

    vkCmdBindDescriptorSets(
        m_handle, static_cast<VkPipelineBindPoint>(bind_point), layout->m_handle, first_set, sets.size(), handles.data(), 0, nullptr);
}

void CommandList::set_descriptor_buffers(eastl::span<DescriptorBufferBindInfo> binding_infos)
{
    ZoneScoped;

    vkCmdBindDescriptorBuffersEXT(m_handle, binding_infos.size(), binding_infos.data());
}

void CommandList::set_descriptor_buffer_offsets(
    PipelineLayout *layout, PipelineBindPoint bind_point, u32 first_set, eastl::span<u32> indices, eastl::span<u64> offsets)
{
    ZoneScoped;

    vkCmdSetDescriptorBufferOffsetsEXT(
        m_handle, static_cast<VkPipelineBindPoint>(bind_point), layout->m_handle, first_set, indices.size(), indices.data(), offsets.data());
}

//////////////////////////////////////////////////
/// COMMAND BATCHER

CommandBatcher::CommandBatcher(CommandList *command_list)
    : m_command_list(command_list)
{
}

CommandBatcher::~CommandBatcher()
{
    flush_barriers();
}

void CommandBatcher::insert_memory_barrier(MemoryBarrier &barrier)
{
    ZoneScoped;

    if (m_memory_barriers.full())
        flush_barriers();

    m_memory_barriers.push_back(barrier);
}

void CommandBatcher::insert_image_barrier(ImageBarrier &barrier)
{
    ZoneScoped;

    if (m_image_barriers.full())
        flush_barriers();

    m_image_barriers.push_back(barrier);
}

void CommandBatcher::flush_barriers()
{
    ZoneScoped;

    if (m_image_barriers.empty() && m_memory_barriers.empty())
        return;

    DependencyInfo dep_info(m_image_barriers, m_memory_barriers);
    m_command_list->set_pipeline_barrier(&dep_info);

    m_image_barriers.clear();
    m_memory_barriers.clear();
}

CommandListSubmitDesc::CommandListSubmitDesc(CommandList *list)
    : VkCommandBufferSubmitInfo(VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO)
{
    this->commandBuffer = list->m_handle;
}

}  // namespace lr::Graphics
