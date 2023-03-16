#include "VKCommandList.hh"

#include "VKAPI.hh"
#include "VKResource.hh"

namespace lr::Graphics
{
    SemaphoreSubmitDesc::SemaphoreSubmitDesc(Semaphore *pSemaphore, u64 value, PipelineStage stage)
    {
        this->sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        this->pNext = nullptr;
        this->semaphore = pSemaphore->m_pHandle;
        this->value = value;
        this->stageMask = VKAPI::ToVKPipelineStage(stage);
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

    void CommandList::BeginPass(RenderPassBeginDesc *pDesc)
    {
        ZoneScoped;

        static constexpr VkAttachmentLoadOp kLoadOpLUT[] = {
            VK_ATTACHMENT_LOAD_OP_LOAD,       // Load
            VK_ATTACHMENT_LOAD_OP_MAX_ENUM,   // Store
            VK_ATTACHMENT_LOAD_OP_CLEAR,      // Clear
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,  // DontCare
        };
        static constexpr VkAttachmentStoreOp kStoreOpLUT[] = {
            VK_ATTACHMENT_STORE_OP_MAX_ENUM,   // Load
            VK_ATTACHMENT_STORE_OP_STORE,      // Store
            VK_ATTACHMENT_STORE_OP_MAX_ENUM,   // Clear
            VK_ATTACHMENT_STORE_OP_DONT_CARE,  // DontCare
        };

        VkRenderingInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        beginInfo.pNext = nullptr;
        beginInfo.viewMask = 0;  // we don't support multi views yet
        beginInfo.layerCount = 1;
        beginInfo.pDepthAttachment = nullptr;

        VkRenderingAttachmentInfo pColorAttachments[LR_MAX_RENDER_TARGET_PER_PASS] = {};
        VkRenderingAttachmentInfo depthAttachment = {};

        for (u32 i = 0; i < pDesc->m_ColorAttachments.size(); i++)
        {
            VkRenderingAttachmentInfo &info = pColorAttachments[i];
            ColorAttachment &attachment = pDesc->m_ColorAttachments[i];
            Image *pImage = attachment.m_pImage;

            info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            info.pNext = nullptr;
            info.imageView = pImage->m_pViewHandle;
            info.imageLayout = VKAPI::ToVKImageLayout(pImage->m_Layout);
            info.loadOp = kLoadOpLUT[(u32)attachment.m_LoadOp];
            info.storeOp = kStoreOpLUT[(u32)attachment.m_StoreOp];

            memcpy(&info.clearValue.color, &attachment.m_ClearValue, sizeof(ColorClearValue));
        }

        if (pDesc->m_pDepthAttachment)
        {
            DepthAttachment &attachment = *pDesc->m_pDepthAttachment;
            Image *pImage = attachment.m_pImage;

            depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            depthAttachment.pNext = nullptr;
            depthAttachment.imageView = pImage->m_pViewHandle;
            depthAttachment.imageLayout = VKAPI::ToVKImageLayout(pImage->m_Layout);
            depthAttachment.loadOp = kLoadOpLUT[(u32)attachment.m_LoadOp];
            depthAttachment.storeOp = kStoreOpLUT[(u32)attachment.m_StoreOp];

            memcpy(&depthAttachment.clearValue.depthStencil, &attachment.m_ClearValue, sizeof(DepthClearValue));

            beginInfo.pDepthAttachment = &depthAttachment;
        }

        beginInfo.colorAttachmentCount = pDesc->m_ColorAttachments.size();
        beginInfo.pColorAttachments = pColorAttachments;
        beginInfo.renderArea.offset.x = (i32)pDesc->m_RenderArea.x;
        beginInfo.renderArea.offset.y = (i32)pDesc->m_RenderArea.y;
        beginInfo.renderArea.extent.width = pDesc->m_RenderArea.z;
        beginInfo.renderArea.extent.height = pDesc->m_RenderArea.w;

        vkCmdBeginRendering(m_pHandle, &beginInfo);
    }

    void CommandList::EndPass()
    {
        ZoneScoped;

        vkCmdEndRendering(m_pHandle);
    }

    void CommandList::SetMemoryBarrier(PipelineBarrier *pBarrier)
    {
        ZoneScoped;

        VkMemoryBarrier2 memoryBarrier = {};
        memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;

        memoryBarrier.srcStageMask = VKAPI::ToVKPipelineStage(pBarrier->m_CurrentStage);
        memoryBarrier.dstStageMask = VKAPI::ToVKPipelineStage(pBarrier->m_NextStage);

        memoryBarrier.srcAccessMask = VKAPI::ToVKAccessFlags(pBarrier->m_CurrentAccess);
        memoryBarrier.dstAccessMask = VKAPI::ToVKAccessFlags(pBarrier->m_NextAccess);

        VkDependencyInfo dependencyInfo = {};
        dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        dependencyInfo.pNext = nullptr;
        dependencyInfo.dependencyFlags = 0;
        dependencyInfo.memoryBarrierCount = 1;
        dependencyInfo.pMemoryBarriers = &memoryBarrier;

        vkCmdPipelineBarrier2(m_pHandle, &dependencyInfo);
    }

    void CommandList::SetImageBarrier(Image *pImage, PipelineBarrier *pBarrier)
    {
        ZoneScoped;

        // TODO: Implement `VkImageSubresourceRange` fully
        VkImageSubresourceRange subresRange = {};
        subresRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        subresRange.baseArrayLayer = 0;
        subresRange.layerCount = 1;
        subresRange.baseMipLevel = 0;
        subresRange.levelCount = pImage->m_MipMapLevels;

        VkImageMemoryBarrier2 barrierInfo = {};
        barrierInfo.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        barrierInfo.pNext = nullptr;

        barrierInfo.image = pImage->m_pHandle;
        barrierInfo.subresourceRange = subresRange;

        barrierInfo.srcStageMask = VKAPI::ToVKPipelineStage(pBarrier->m_CurrentStage);
        barrierInfo.dstStageMask = VKAPI::ToVKPipelineStage(pBarrier->m_NextStage);

        barrierInfo.srcAccessMask = VKAPI::ToVKAccessFlags(pBarrier->m_CurrentAccess);
        barrierInfo.dstAccessMask = VKAPI::ToVKAccessFlags(pBarrier->m_NextAccess);

        barrierInfo.oldLayout = VKAPI::ToVKImageLayout(pBarrier->m_CurrentLayout);
        barrierInfo.newLayout = VKAPI::ToVKImageLayout(pBarrier->m_NextLayout);

        barrierInfo.srcQueueFamilyIndex = m_pQueueInfo->m_Indexes[pBarrier->m_CurrentQueue];
        barrierInfo.dstQueueFamilyIndex = m_pQueueInfo->m_Indexes[pBarrier->m_NextQueue];

        VkDependencyInfo dependencyInfo = {};
        dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        dependencyInfo.pNext = nullptr;
        dependencyInfo.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
        dependencyInfo.imageMemoryBarrierCount = 1;
        dependencyInfo.pImageMemoryBarriers = &barrierInfo;

        vkCmdPipelineBarrier2(m_pHandle, &dependencyInfo);

        pImage->m_Layout = pBarrier->m_NextLayout;
    }

    void CommandList::SetBufferBarrier(Buffer *pBuffer, PipelineBarrier *pBarrier)
    {
        ZoneScoped;

        VkBufferMemoryBarrier2 barrierInfo = {};
        barrierInfo.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
        barrierInfo.pNext = nullptr;

        barrierInfo.buffer = pBuffer->m_pHandle;
        barrierInfo.offset = pBuffer->m_DeviceDataOffset;
        barrierInfo.size = VK_WHOLE_SIZE;

        barrierInfo.srcStageMask = VKAPI::ToVKPipelineStage(pBarrier->m_CurrentStage);
        barrierInfo.dstStageMask = VKAPI::ToVKPipelineStage(pBarrier->m_NextStage);

        barrierInfo.srcAccessMask = VKAPI::ToVKAccessFlags(pBarrier->m_CurrentAccess);
        barrierInfo.dstAccessMask = VKAPI::ToVKAccessFlags(pBarrier->m_NextAccess);

        barrierInfo.srcQueueFamilyIndex = m_pQueueInfo->m_Indexes[pBarrier->m_CurrentQueue];
        barrierInfo.dstQueueFamilyIndex = m_pQueueInfo->m_Indexes[pBarrier->m_NextQueue];

        VkDependencyInfo dependencyInfo = {};
        dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        dependencyInfo.pNext = nullptr;
        dependencyInfo.dependencyFlags = 0;
        dependencyInfo.bufferMemoryBarrierCount = 1;
        dependencyInfo.pBufferMemoryBarriers = &barrierInfo;

        vkCmdPipelineBarrier2(m_pHandle, &dependencyInfo);
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

    void CommandList::CopyBuffer(Buffer *pSource, Buffer *pDest, u32 size)
    {
        ZoneScoped;

        VkBufferCopy copyRegion = {};
        copyRegion.size = size;

        vkCmdCopyBuffer(m_pHandle, pSource->m_pHandle, pDest->m_pHandle, 1, &copyRegion);
    }

    void CommandList::CopyBuffer(Buffer *pSource, Image *pDest)
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
            m_pHandle,
            pSource->m_pHandle,
            pDest->m_pHandle,
            VKAPI::ToVKImageLayout(pDest->m_Layout),
            1,
            &imageCopyInfo);
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

        vkCmdSetPrimitiveTopology(m_pHandle, VKAPI::ToVKTopology(type));
    }

    void CommandList::SetPipeline(Pipeline *pPipeline)
    {
        ZoneScoped;

        m_pPipeline = pPipeline;

        vkCmdBindPipeline(m_pHandle, m_pPipeline->m_BindPoint, pPipeline->m_pHandle);
    }

    void CommandList::SetDescriptorSets(const std::initializer_list<DescriptorSet *> &sets)
    {
        ZoneScoped;

        u32 idx = 0;
        VkDescriptorSet pSets[LR_MAX_DESCRIPTOR_SETS_PER_PIPELINE];
        for (auto &pSet : sets)
            pSets[idx++] = pSet->m_pHandle;

        vkCmdBindDescriptorSets(
            m_pHandle, m_pPipeline->m_BindPoint, m_pPipeline->m_pLayout, 0, sets.size(), pSets, 0, nullptr);
    }

    void CommandList::SetPushConstants(ShaderStage stage, u32 offset, void *pData, u32 dataSize)
    {
        ZoneScoped;

        vkCmdPushConstants(
            m_pHandle, m_pPipeline->m_pLayout, VKAPI::ToVKShaderType(stage), offset, dataSize, pData);
    }

    CommandListSubmitDesc::CommandListSubmitDesc(CommandList *pList)
    {
        this->sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
        this->pNext = nullptr;
        this->commandBuffer = pList->m_pHandle;
        this->deviceMask = 0;
    }

}  // namespace lr::Graphics