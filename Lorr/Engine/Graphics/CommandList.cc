// Created on Monday July 18th 2022 by exdal
// Last modified on Friday May 26th 2023 by exdal

#include "CommandList.hh"

#include "Buffer.hh"
#include "Image.hh"
#include "VulkanType.hh"

namespace lr::Graphics
{
ImageBarrier::ImageBarrier(Image *pImage, const PipelineBarrier &barrier)
{
    ZoneScoped;

    VkImageSubresourceRange subresRange = {
        .aspectMask = pImage->m_ImageAspect,
        .baseMipLevel = 0,
        .levelCount = pImage->m_MipMapLevels,
        .baseArrayLayer = 0,
        .layerCount = 1,
    };

    this->sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    this->pNext = nullptr;
    this->srcStageMask = VK::ToPipelineStage(barrier.m_SrcStage);
    this->srcAccessMask = VK::ToAccessFlags(barrier.m_SrcAccess);
    this->dstStageMask = VK::ToPipelineStage(barrier.m_DstStage);
    this->dstAccessMask = VK::ToAccessFlags(barrier.m_DstAccess);
    this->oldLayout = VK::ToImageLayout(barrier.m_SrcLayout);
    this->newLayout = VK::ToImageLayout(barrier.m_DstLayout);
    this->srcQueueFamilyIndex = barrier.m_SrcQueue;
    this->dstQueueFamilyIndex = barrier.m_DstQueue;

    this->image = pImage->m_pHandle;
    this->subresourceRange = subresRange;
}

BufferBarrier::BufferBarrier(Buffer *pBuffer, const PipelineBarrier &barrier)
{
    ZoneScoped;

    this->sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
    this->pNext = nullptr;
    this->srcStageMask = VK::ToPipelineStage(barrier.m_SrcStage);
    this->dstStageMask = VK::ToPipelineStage(barrier.m_DstStage);
    this->srcAccessMask = VK::ToAccessFlags(barrier.m_SrcAccess);
    this->dstAccessMask = VK::ToAccessFlags(barrier.m_DstAccess);
    this->srcQueueFamilyIndex = barrier.m_SrcQueue;
    this->dstQueueFamilyIndex = barrier.m_DstQueue;

    this->buffer = pBuffer->m_pHandle;
    this->offset = pBuffer->m_AllocatorOffset;
    this->size = VK_WHOLE_SIZE;  // TODO
}

MemoryBarrier::MemoryBarrier(const PipelineBarrier &barrier)
{
    ZoneScoped;

    this->sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
    this->pNext = nullptr;
    this->srcStageMask = VK::ToPipelineStage(barrier.m_SrcStage);
    this->dstStageMask = VK::ToPipelineStage(barrier.m_DstStage);
    this->srcAccessMask = VK::ToAccessFlags(barrier.m_SrcAccess);
    this->dstAccessMask = VK::ToAccessFlags(barrier.m_DstAccess);
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

DependencyInfo::DependencyInfo(eastl::span<ImageBarrier> imageBarriers)
    : DependencyInfo()
{
    ZoneScoped;

    SetImageBarriers(imageBarriers);
}

DependencyInfo::DependencyInfo(eastl::span<BufferBarrier> bufferBarriers)
    : DependencyInfo()
{
    ZoneScoped;

    SetBufferBarriers(bufferBarriers);
}

DependencyInfo::DependencyInfo(eastl::span<MemoryBarrier> memoryBarriers)
    : DependencyInfo()
{
    ZoneScoped;

    SetMemoryBarriers(memoryBarriers);
}

void DependencyInfo::SetImageBarriers(eastl::span<ImageBarrier> imageBarriers)
{
    ZoneScoped;

    this->imageMemoryBarrierCount = imageBarriers.size();
    this->pImageMemoryBarriers = imageBarriers.data();
}

void DependencyInfo::SetBufferBarriers(eastl::span<BufferBarrier> bufferBarriers)
{
    ZoneScoped;

    this->bufferMemoryBarrierCount = bufferBarriers.size();
    this->pBufferMemoryBarriers = bufferBarriers.data();
}

void DependencyInfo::SetMemoryBarriers(eastl::span<MemoryBarrier> memoryBarriers)
{
    ZoneScoped;

    this->memoryBarrierCount = memoryBarriers.size();
    this->pMemoryBarriers = memoryBarriers.data();
}

SemaphoreSubmitDesc::SemaphoreSubmitDesc(Semaphore *pSemaphore, u64 value, PipelineStage stage)
{
    this->sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    this->pNext = nullptr;
    this->semaphore = pSemaphore->m_pHandle;
    this->value = value;
    this->stageMask = VK::ToPipelineStage(stage);
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

void CommandList::BeginRendering(RenderingBeginDesc *pDesc)
{
    ZoneScoped;

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

    VkRenderingInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    beginInfo.pNext = nullptr;
    beginInfo.viewMask = 0;  // we don't support multi views yet
    beginInfo.layerCount = 1;
    beginInfo.pDepthAttachment = nullptr;

    VkRenderingAttachmentInfo pColorAttachments[LR_MAX_COLOR_ATTACHMENT_PER_PASS] = {};
    VkRenderingAttachmentInfo depthAttachment = {};

    for (u32 i = 0; i < pDesc->m_ColorAttachments.size(); i++)
    {
        VkRenderingAttachmentInfo &colorAttachment = pColorAttachments[i];
        RenderingColorAttachment &attachment = pDesc->m_ColorAttachments[i];
        Image *pImage = attachment.m_pImage;

        colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        colorAttachment.pNext = nullptr;
        colorAttachment.imageView = pImage->m_pViewHandle;
        colorAttachment.imageLayout = VK::ToImageLayout(attachment.m_Layout);
        colorAttachment.loadOp = kLoadOpLUT[(u32)attachment.m_LoadOp];
        colorAttachment.storeOp = kStoreOpLUT[(u32)attachment.m_StoreOp];

        memcpy(&colorAttachment.clearValue.color, &attachment.m_ClearValue, sizeof(ColorClearValue));
    }

    if (pDesc->m_pDepthAttachment)
    {
        beginInfo.pDepthAttachment = &depthAttachment;

        RenderingDepthAttachment &attachment = *pDesc->m_pDepthAttachment;
        Image *pImage = attachment.m_pImage;

        depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        depthAttachment.pNext = nullptr;
        depthAttachment.imageView = pImage->m_pViewHandle;
        depthAttachment.imageLayout = VK::ToImageLayout(attachment.m_Layout);
        depthAttachment.loadOp = kLoadOpLUT[(u32)attachment.m_LoadOp];
        depthAttachment.storeOp = kStoreOpLUT[(u32)attachment.m_StoreOp];

        memcpy(&depthAttachment.clearValue.depthStencil, &attachment.m_ClearValue, sizeof(DepthClearValue));
    }

    beginInfo.colorAttachmentCount = pDesc->m_ColorAttachments.size();
    beginInfo.pColorAttachments = pColorAttachments;
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

void CommandList::SetVertexBuffer(Buffer *pBuffer)
{
    ZoneScoped;

    VkDeviceSize pOffsets[1] = { 0 };
    vkCmdBindVertexBuffers(m_pHandle, 0, 1, &pBuffer->m_pHandle, pOffsets);
}

void CommandList::SetIndexBuffer(Buffer *pBuffer, bool type32)
{
    ZoneScoped;

    vkCmdBindIndexBuffer(
        m_pHandle, pBuffer->m_pHandle, 0, type32 ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16);
}

void CommandList::CopyBuffer(Buffer *pSource, Buffer *pDest, u64 srcOff, u64 dstOff, u64 size)
{
    ZoneScoped;

    VkBufferCopy copyRegion = { .srcOffset = srcOff, .dstOffset = dstOff, .size = size };
    vkCmdCopyBuffer(m_pHandle, pSource->m_pHandle, pDest->m_pHandle, 1, &copyRegion);
}

void CommandList::CopyBuffer(Buffer *pSource, Image *pDest, ImageLayout layout)
{
    ZoneScoped;

    // TODO: Multiple mip levels
    VkBufferImageCopy imageCopyInfo = {};
    imageCopyInfo.bufferOffset = 0;

    imageCopyInfo.imageOffset.x = 0;
    imageCopyInfo.imageOffset.y = 0;
    imageCopyInfo.imageOffset.z = 0;

    imageCopyInfo.imageExtent.width = pDest->m_Width;
    imageCopyInfo.imageExtent.height = pDest->m_Height;
    imageCopyInfo.imageExtent.depth = 1;

    imageCopyInfo.imageSubresource.aspectMask = pDest->m_ImageAspect;
    imageCopyInfo.imageSubresource.baseArrayLayer = 0;
    imageCopyInfo.imageSubresource.layerCount = 1;
    imageCopyInfo.imageSubresource.mipLevel = 0;

    vkCmdCopyBufferToImage(
        m_pHandle, pSource->m_pHandle, pDest->m_pHandle, VK::ToImageLayout(layout), 1, &imageCopyInfo);
}

void CommandList::Draw(u32 vertexCount, u32 firstVertex, u32 instanceCount, u32 firstInstance)
{
    ZoneScoped;

    vkCmdDraw(m_pHandle, vertexCount, instanceCount, firstVertex, firstInstance);
}

void CommandList::DrawIndexed(
    u32 indexCount, u32 firstIndex, u32 vertexOffset, u32 instanceCount, u32 firstInstance)
{
    ZoneScoped;

    vkCmdDrawIndexed(m_pHandle, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
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

    vkCmdSetPrimitiveTopology(m_pHandle, VK::ToTopology(type));
}

void CommandList::SetPipeline(Pipeline *pPipeline)
{
    ZoneScoped;

    m_pPipeline = pPipeline;

    vkCmdBindPipeline(m_pHandle, m_pPipeline->m_BindPoint, pPipeline->m_pHandle);
}

void CommandList::SetPushConstants(ShaderStage stage, u32 offset, void *pData, u32 dataSize)
{
    ZoneScoped;

    vkCmdPushConstants(
        m_pHandle, m_pPipeline->m_pLayout->m_pHandle, VK::ToShaderType(stage), offset, dataSize, pData);
}

void CommandList::SetDescriptorBuffers(eastl::span<DescriptorBindingInfo> bindingInfos)
{
    ZoneScoped;

    vkCmdBindDescriptorBuffersEXT(m_pHandle, bindingInfos.size(), bindingInfos.data());
}

void CommandList::SetDescriptorBufferOffsets(
    u32 firstSet, u32 setCount, eastl::span<u32> indices, eastl::span<u64> offsets)
{
    ZoneScoped;
    
    vkCmdSetDescriptorBufferOffsetsEXT(
        m_pHandle,
        m_pPipeline->m_BindPoint,
        m_pPipeline->m_pLayout->m_pHandle,
        firstSet,
        setCount,
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