#include "CommandList.hh"

namespace lr::Graphics
{
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

DescriptorBufferBindInfo::DescriptorBufferBindInfo(
    Buffer *pBuffer, BufferUsage bufferUsage)
{
    this->sType = VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT;
    this->pNext = nullptr;
    this->address = pBuffer->m_DeviceAddress;
    this->usage = (VkBufferUsageFlags)bufferUsage;
}

RenderingAttachment::RenderingAttachment(
    ImageView *pImageView, ImageLayout layout, AttachmentOp loadOp, AttachmentOp storeOp)
{
    this->sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    this->pNext = nullptr;
    this->imageView = *pImageView;
    this->imageLayout = static_cast<VkImageLayout>(layout);
    this->resolveMode = VK_RESOLVE_MODE_NONE;
    this->resolveImageView = nullptr;
    this->resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    this->loadOp = kLoadOpLUT[(u32)loadOp];
    this->storeOp = kStoreOpLUT[(u32)storeOp];
}

RenderingAttachment::RenderingAttachment(
    ImageView *pImageView,
    ImageLayout layout,
    AttachmentOp loadOp,
    AttachmentOp storeOp,
    ColorClearValue clearVal)
    : RenderingAttachment(pImageView, layout, loadOp, storeOp)
{
    memcpy(&this->clearValue.color, &clearVal, sizeof(ColorClearValue));
}

RenderingAttachment::RenderingAttachment(
    ImageView *pImageView,
    ImageLayout layout,
    AttachmentOp loadOp,
    AttachmentOp storeOp,
    DepthClearValue clearVal)
    : RenderingAttachment(pImageView, layout, loadOp, storeOp)
{
    memcpy(&this->clearValue.color, &clearVal, sizeof(DepthClearValue));
}

ImageBarrier::ImageBarrier(
    Image *pImage, ImageSubresourceInfo sliceInfo, const PipelineBarrier &barrier)
{
    ZoneScoped;

    this->sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    this->pNext = nullptr;
    this->srcStageMask = (VkPipelineStageFlags2)barrier.m_SrcStage;
    this->srcAccessMask = (VkAccessFlags2)barrier.m_SrcAccess;
    this->dstStageMask = (VkPipelineStageFlags2)barrier.m_DstStage;
    this->dstAccessMask = (VkAccessFlags2)barrier.m_DstAccess;
    this->oldLayout = (VkImageLayout)barrier.m_SrcLayout;
    this->newLayout = (VkImageLayout)barrier.m_DstLayout;
    this->srcQueueFamilyIndex = barrier.m_SrcQueueIndex;
    this->dstQueueFamilyIndex = barrier.m_DstQueueIndex;
    this->image = *pImage;
    this->subresourceRange = ImageSubresourceRange(sliceInfo);
}

MemoryBarrier::MemoryBarrier(const PipelineBarrier &barrier)
{
    ZoneScoped;

    this->sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
    this->pNext = nullptr;
    this->srcStageMask = (VkPipelineStageFlags2)barrier.m_SrcStage;
    this->dstStageMask = (VkPipelineStageFlags2)barrier.m_DstStage;
    this->srcAccessMask = (VkAccessFlags2)barrier.m_SrcAccess;
    this->dstAccessMask = (VkAccessFlags2)barrier.m_DstAccess;
}

DependencyInfo::DependencyInfo()
{
    this->sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    this->pNext = nullptr;
    this->dependencyFlags = 0;
    this->memoryBarrierCount = 0;
    this->pMemoryBarriers = nullptr;
    this->bufferMemoryBarrierCount = 0;
    this->pBufferMemoryBarriers = nullptr;
    this->imageMemoryBarrierCount = 0;
    this->pImageMemoryBarriers = nullptr;
}

DependencyInfo::DependencyInfo(
    eastl::span<ImageBarrier> imageBarriers, eastl::span<MemoryBarrier> memoryBarriers)
    : DependencyInfo()
{
    ZoneScoped;

    this->imageMemoryBarrierCount = imageBarriers.size();
    this->pImageMemoryBarriers = imageBarriers.data();
    this->memoryBarrierCount = memoryBarriers.size();
    this->pMemoryBarriers = memoryBarriers.data();
}

SemaphoreSubmitDesc::SemaphoreSubmitDesc(Semaphore *pSemaphore, PipelineStage stage)
{
    this->sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    this->pNext = nullptr;
    this->semaphore = pSemaphore->m_pHandle;
    this->value = 0;
    this->stageMask = (VkPipelineStageFlags2)stage;
    this->deviceIndex = 0;
}

SemaphoreSubmitDesc::SemaphoreSubmitDesc(
    Semaphore *pSemaphore, u64 value, PipelineStage stage)
{
    this->sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    this->pNext = nullptr;
    this->semaphore = pSemaphore->m_pHandle;
    this->value = value;
    this->stageMask = (VkPipelineStageFlags2)stage;
    this->deviceIndex = 0;
}

SemaphoreSubmitDesc::SemaphoreSubmitDesc(Semaphore *pSemaphore, u64 value)
{
    this->sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    this->pNext = nullptr;
    this->semaphore = pSemaphore->m_pHandle;
    this->value = value;
    this->stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    this->deviceIndex = 0;
}

BufferCopyRegion::BufferCopyRegion(u64 srcOffset, u64 dstOffset, u64 size)
{
    this->srcOffset = srcOffset;
    this->dstOffset = dstOffset;
    this->size = size;
}

ImageCopyRegion::ImageCopyRegion(const VkBufferImageCopy &lazy)
{
    *this = lazy;
}

ImageCopyRegion::ImageCopyRegion(
    ImageSubresourceInfo sliceInfo, u32 width, u32 height, u64 bufferOffset)
    : ImageCopyRegion({})
{
    VkImageSubresourceLayers layerInfo = {
        .aspectMask = static_cast<VkImageAspectFlags>(sliceInfo.m_AspectMask),
        .mipLevel = sliceInfo.m_BaseMip,
        .baseArrayLayer = sliceInfo.m_BaseSlice,
        .layerCount = sliceInfo.m_SliceCount,
    };

    this->bufferOffset = bufferOffset;
    this->imageSubresource = layerInfo;
    this->imageExtent.width = width;
    this->imageExtent.height = height;
    this->imageExtent.depth = 1;
}

void CommandList::BeginRendering(RenderingBeginDesc *pDesc)
{
    ZoneScoped;

    VkRenderingInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .pNext = nullptr,
        .layerCount = 1,
        .viewMask = 0,  // TODO: Multiviews
        .colorAttachmentCount = (u32)pDesc->m_ColorAttachments.size(),
        .pColorAttachments = pDesc->m_ColorAttachments.data(),
        .pDepthAttachment = pDesc->m_pDepthAttachment,
    };
    beginInfo.renderArea.offset.x = (i32)pDesc->m_RenderArea.x;
    beginInfo.renderArea.offset.y = (i32)pDesc->m_RenderArea.y;
    beginInfo.renderArea.extent.width = pDesc->m_RenderArea.z;
    beginInfo.renderArea.extent.height = pDesc->m_RenderArea.w;

    vkCmdBeginRendering(m_pHandle, &beginInfo);
}

void CommandList::EndRendering()
{
    ZoneScoped;

    vkCmdEndRendering(m_pHandle);
}

void CommandList::SetPipelineBarrier(DependencyInfo *pDependencyInfo)
{
    ZoneScoped;

    vkCmdPipelineBarrier2(m_pHandle, pDependencyInfo);
}

void CommandList::CopyBufferToBuffer(
    Buffer *pSrc, Buffer *pDst, eastl::span<BufferCopyRegion> regions)
{
    ZoneScoped;

    vkCmdCopyBuffer(m_pHandle, *pSrc, *pDst, regions.size(), regions.data());
}

void CommandList::CopyBufferToImage(
    Buffer *pSrc, Image *pDst, ImageLayout layout, eastl::span<ImageCopyRegion> regions)
{
    ZoneScoped;

    vkCmdCopyBufferToImage(
        m_pHandle,
        *pSrc,
        *pDst,
        static_cast<VkImageLayout>(layout),
        regions.size(),
        regions.data());
}

void CommandList::Draw(
    u32 vertexCount, u32 firstVertex, u32 instanceCount, u32 firstInstance)
{
    ZoneScoped;

    vkCmdDraw(m_pHandle, vertexCount, instanceCount, firstVertex, firstInstance);
}

void CommandList::DrawIndexed(
    u32 indexCount,
    u32 firstIndex,
    u32 vertexOffset,
    u32 instanceCount,
    u32 firstInstance)
{
    ZoneScoped;

    vkCmdDrawIndexed(
        m_pHandle, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

void CommandList::Dispatch(u32 groupX, u32 groupY, u32 groupZ)
{
    ZoneScoped;

    vkCmdDispatch(m_pHandle, groupX, groupY, groupZ);
}

void CommandList::SetViewport(u32 id, u32 x, u32 y, u32 width, u32 height)
{
    ZoneScoped;

    VkViewport vp;
    vp.x = x;
    vp.y = y;
    vp.width = width;
    vp.height = height;
    vp.minDepth = 0.0;
    vp.maxDepth = 1.0;

    vkCmdSetViewport(m_pHandle, id, 1, &vp);
}

void CommandList::SetScissors(u32 id, u32 x, u32 y, u32 width, u32 height)
{
    ZoneScoped;

    VkRect2D rect;
    rect.offset.x = x;
    rect.offset.y = y;
    rect.extent.width = width - x;
    rect.extent.height = height - y;

    vkCmdSetScissor(m_pHandle, id, 1, &rect);
}

void CommandList::SetPrimitiveType(PrimitiveType type)
{
    ZoneScoped;

    vkCmdSetPrimitiveTopology(m_pHandle, (VkPrimitiveTopology)type);
}

void CommandList::SetPipeline(Pipeline *pPipeline)
{
    ZoneScoped;

    vkCmdBindPipeline(m_pHandle, pPipeline->m_BindPoint, *pPipeline);
}

void CommandList::SetPushConstants(
    void *pData, u32 dataSize, u32 offset, PipelineLayout layout, ShaderStage stageFlags)
{
    ZoneScoped;

    vkCmdPushConstants(
        m_pHandle,
        layout,
        static_cast<VkShaderStageFlags>(stageFlags),
        offset,
        dataSize,
        pData);
}

void CommandList::SetDescriptorBuffers(eastl::span<DescriptorBufferBindInfo> bindingInfos)
{
    ZoneScoped;

    vkCmdBindDescriptorBuffersEXT(m_pHandle, bindingInfos.size(), bindingInfos.data());
}

void CommandList::SetDescriptorBufferOffsets(
    PipelineBindPoint bindPoint,
    PipelineLayout layout,
    u32 firstSet,
    eastl::span<u32> indices,
    eastl::span<u64> offsets)
{
    ZoneScoped;

    vkCmdSetDescriptorBufferOffsetsEXT(
        m_pHandle,
        static_cast<VkPipelineBindPoint>(bindPoint),
        layout,
        firstSet,
        indices.size(),
        indices.data(),
        offsets.data());
}

CommandListSubmitDesc::CommandListSubmitDesc(CommandList *pList)
{
    this->sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    this->pNext = nullptr;
    this->commandBuffer = pList->m_pHandle;
    this->deviceMask = 0;
}

}  // namespace lr::Graphics