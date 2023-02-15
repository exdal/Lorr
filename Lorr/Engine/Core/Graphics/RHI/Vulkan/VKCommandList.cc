#include "VKCommandList.hh"

#include "Engine.hh"

#include "VKAPI.hh"
#include "VKResource.hh"

#define API_VAR(type, name) type *name##VK = (type *)name

namespace lr::Graphics
{
    void VKCommandList::Init(VkCommandBuffer pHandle, VkFence pFence, CommandListType type)
    {
        ZoneScoped;

        m_pHandle = pHandle;
        m_pFence = pFence;

        m_Type = type;
    }

    void VKCommandList::BeginPass(CommandListBeginDesc *pDesc)
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

        for (u32 i = 0; i < pDesc->m_ColorAttachmentCount; i++)
        {
            VkRenderingAttachmentInfo &info = pColorAttachments[i];
            CommandListAttachment &attachment = pDesc->m_pColorAttachments[i];

            Image *pImage = attachment.m_pHandle;
            API_VAR(VKImage, pImage);

            info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            info.pNext = nullptr;

            info.imageView = pImageVK->m_pViewHandle;
            info.imageLayout = pImageVK->m_Layout;
            info.loadOp = kLoadOpLUT[(u32)attachment.m_LoadOp];
            info.storeOp = kStoreOpLUT[(u32)attachment.m_StoreOp];

            memcpy(&info.clearValue.color, &attachment.m_ClearVal.m_RenderTargetColor, sizeof(XMFLOAT4));
        }

        if (pDesc->m_pDepthAttachment)
        {
            CommandListAttachment &attachment = *pDesc->m_pDepthAttachment;

            Image *pImage = attachment.m_pHandle;
            API_VAR(VKImage, pImage);

            depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            depthAttachment.pNext = nullptr;

            depthAttachment.imageView = pImageVK->m_pViewHandle;
            depthAttachment.imageLayout = pImageVK->m_Layout;
            depthAttachment.loadOp = kLoadOpLUT[(u32)attachment.m_LoadOp];
            depthAttachment.storeOp = kStoreOpLUT[(u32)attachment.m_StoreOp];

            memcpy(
                &depthAttachment.clearValue.color,
                &attachment.m_ClearVal.m_RenderTargetColor,
                sizeof(XMFLOAT4));

            beginInfo.pDepthAttachment = &depthAttachment;
        }

        beginInfo.colorAttachmentCount = pDesc->m_ColorAttachmentCount;
        beginInfo.pColorAttachments = pColorAttachments;
        beginInfo.renderArea.offset.x = (i32)pDesc->m_RenderArea.x;
        beginInfo.renderArea.offset.y = (i32)pDesc->m_RenderArea.y;
        beginInfo.renderArea.extent.width = pDesc->m_RenderArea.z;
        beginInfo.renderArea.extent.height = pDesc->m_RenderArea.w;

        vkCmdBeginRendering(m_pHandle, &beginInfo);
    }

    void VKCommandList::EndPass()
    {
        ZoneScoped;

        vkCmdEndRendering(m_pHandle);
    }

    void VKCommandList::ClearImage(Image *pImage, ClearValue val)
    {
        ZoneScoped;

        API_VAR(VKImage, pImage);

        VkClearValue color;
        memcpy(color.color.float32, &val.m_RenderTargetColor.x, sizeof(XMFLOAT4));

        VkImageSubresourceRange subresRange = {};
        subresRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        subresRange.baseArrayLayer = 0;
        subresRange.layerCount = 1;
        subresRange.baseMipLevel = 0;
        subresRange.levelCount = pImageVK->m_MipMapLevels;

        vkCmdClearColorImage(
            m_pHandle,
            pImageVK->m_pHandle,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            (const VkClearColorValue *)&color,
            1,
            &subresRange);
    }

    void VKCommandList::SetImageBarrier(Image *pImage, PipelineBarrier *pBarrier)
    {
        ZoneScoped;

        API_VAR(VKImage, pImage);

        // TODO: Implement `VkImageSubresourceRange` fully
        VkImageSubresourceRange subresRange = {};
        subresRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        subresRange.baseArrayLayer = 0;
        subresRange.layerCount = 1;
        subresRange.baseMipLevel = 0;
        subresRange.levelCount = pImageVK->m_MipMapLevels;

        VkImageMemoryBarrier2 barrierInfo = {};
        barrierInfo.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        barrierInfo.pNext = nullptr;

        barrierInfo.image = pImageVK->m_pHandle;
        barrierInfo.subresourceRange = subresRange;

        barrierInfo.srcStageMask = VKAPI::ToVKPipelineStage(pBarrier->m_CurrentStage);
        barrierInfo.dstStageMask = VKAPI::ToVKPipelineStage(pBarrier->m_NextStage);

        barrierInfo.srcAccessMask = VKAPI::ToVKAccessFlags(pBarrier->m_CurrentAccess);
        barrierInfo.dstAccessMask = VKAPI::ToVKAccessFlags(pBarrier->m_NextAccess);

        barrierInfo.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrierInfo.newLayout = VKAPI::ToVKImageLayout(pBarrier->m_NextUsage);

        // We initialize images on Vulkan with UNDEFINED or PREINITIALIZED...
        if (pImageVK->m_Layout != VK_IMAGE_LAYOUT_UNDEFINED)
            barrierInfo.oldLayout = VKAPI::ToVKImageLayout(pBarrier->m_CurrentUsage);

        barrierInfo.srcQueueFamilyIndex = 0;
        barrierInfo.dstQueueFamilyIndex = 0;

        VkDependencyInfo dependencyInfo = {};
        dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        dependencyInfo.pNext = nullptr;
        dependencyInfo.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
        dependencyInfo.imageMemoryBarrierCount = 1;
        dependencyInfo.pImageMemoryBarriers = &barrierInfo;

        vkCmdPipelineBarrier2(m_pHandle, &dependencyInfo);

        pImageVK->m_Layout = barrierInfo.newLayout;
    }

    void VKCommandList::SetBufferBarrier(Buffer *pBuffer, PipelineBarrier *pBarrier)
    {
        ZoneScoped;

        API_VAR(VKBuffer, pBuffer);

        VkBufferMemoryBarrier2 barrierInfo = {};
        barrierInfo.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
        barrierInfo.pNext = nullptr;

        barrierInfo.buffer = pBufferVK->m_pHandle;
        barrierInfo.offset = pBuffer->m_DataOffset;
        barrierInfo.size = VK_WHOLE_SIZE;

        barrierInfo.srcStageMask = VKAPI::ToVKPipelineStage(pBarrier->m_CurrentStage);
        barrierInfo.dstStageMask = VKAPI::ToVKPipelineStage(pBarrier->m_NextStage);

        barrierInfo.srcAccessMask = VKAPI::ToVKAccessFlags(pBarrier->m_CurrentAccess);
        barrierInfo.dstAccessMask = VKAPI::ToVKAccessFlags(pBarrier->m_NextAccess);

        barrierInfo.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrierInfo.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

        VkDependencyInfo dependencyInfo = {};
        dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        dependencyInfo.pNext = nullptr;
        dependencyInfo.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
        dependencyInfo.bufferMemoryBarrierCount = 1;
        dependencyInfo.pBufferMemoryBarriers = &barrierInfo;

        vkCmdPipelineBarrier2(m_pHandle, &dependencyInfo);
    }

    void VKCommandList::SetVertexBuffer(Buffer *pBuffer)
    {
        ZoneScoped;

        API_VAR(VKBuffer, pBuffer);

        VkDeviceSize pOffsets[1] = { 0 };
        vkCmdBindVertexBuffers(m_pHandle, 0, 1, &pBufferVK->m_pHandle, pOffsets);
    }

    void VKCommandList::SetIndexBuffer(Buffer *pBuffer, bool type32)
    {
        ZoneScoped;

        API_VAR(VKBuffer, pBuffer);

        vkCmdBindIndexBuffer(
            m_pHandle, pBufferVK->m_pHandle, 0, type32 ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16);
    }

    void VKCommandList::CopyBuffer(Buffer *pSource, Buffer *pDest, u32 size)
    {
        ZoneScoped;

        API_VAR(VKBuffer, pSource);
        API_VAR(VKBuffer, pDest);

        VkBufferCopy copyRegion = {};
        copyRegion.size = size;

        vkCmdCopyBuffer(m_pHandle, pSourceVK->m_pHandle, pDestVK->m_pHandle, 1, &copyRegion);
    }

    void VKCommandList::CopyBuffer(Buffer *pSource, Image *pDest)
    {
        ZoneScoped;

        API_VAR(VKBuffer, pSource);
        API_VAR(VKImage, pDest);

        VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_NONE;

        if (pDest->m_UsageFlags & LR_RESOURCE_USAGE_SHADER_RESOURCE)
            aspectMask |= VK_IMAGE_ASPECT_COLOR_BIT;
        if (pDest->m_UsageFlags & LR_RESOURCE_USAGE_DEPTH_STENCIL)
            aspectMask |= VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;

        // TODO: Multiple mip levels
        VkBufferImageCopy imageCopyInfo = {};
        imageCopyInfo.bufferOffset = 0;

        imageCopyInfo.imageOffset.x = 0;
        imageCopyInfo.imageOffset.y = 0;
        imageCopyInfo.imageOffset.z = 0;

        imageCopyInfo.imageExtent.width = pDest->m_Width;
        imageCopyInfo.imageExtent.height = pDest->m_Height;
        imageCopyInfo.imageExtent.depth = 1;

        imageCopyInfo.imageSubresource.aspectMask = aspectMask;
        imageCopyInfo.imageSubresource.baseArrayLayer = 0;
        imageCopyInfo.imageSubresource.layerCount = 1;
        imageCopyInfo.imageSubresource.mipLevel = 0;

        vkCmdCopyBufferToImage(
            m_pHandle, pSourceVK->m_pHandle, pDestVK->m_pHandle, pDestVK->m_Layout, 1, &imageCopyInfo);
    }

    void VKCommandList::Draw(u32 vertexCount, u32 firstVertex, u32 instanceCount, u32 firstInstance)
    {
        ZoneScoped;

        vkCmdDraw(m_pHandle, vertexCount, instanceCount, firstVertex, firstInstance);
    }

    void VKCommandList::DrawIndexed(
        u32 indexCount, u32 firstIndex, u32 vertexOffset, u32 instanceCount, u32 firstInstance)
    {
        ZoneScoped;

        vkCmdDrawIndexed(m_pHandle, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
    }

    void VKCommandList::Dispatch(u32 groupX, u32 groupY, u32 groupZ)
    {
        ZoneScoped;

        vkCmdDispatch(m_pHandle, groupX, groupY, groupZ);
    }

    void VKCommandList::SetViewport(u32 id, u32 x, u32 y, u32 width, u32 height)
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

    void VKCommandList::SetScissors(u32 id, u32 x, u32 y, u32 width, u32 height)
    {
        ZoneScoped;

        VkRect2D rect;
        rect.offset.x = x;
        rect.offset.y = y;
        rect.extent.width = width - x;
        rect.extent.height = height - y;

        vkCmdSetScissor(m_pHandle, id, 1, &rect);
    }

    void VKCommandList::SetPrimitiveType(PrimitiveType type)
    {
        ZoneScoped;

        vkCmdSetPrimitiveTopology(m_pHandle, VKAPI::ToVKTopology(type));
    }

    void VKCommandList::SetGraphicsPipeline(Pipeline *pPipeline)
    {
        ZoneScoped;

        API_VAR(VKPipeline, pPipeline);

        m_pSetPipeline = pPipelineVK;
        vkCmdBindPipeline(m_pHandle, VK_PIPELINE_BIND_POINT_GRAPHICS, pPipelineVK->m_pHandle);
    }

    void VKCommandList::SetComputePipeline(Pipeline *pPipeline)
    {
        ZoneScoped;

        API_VAR(VKPipeline, pPipeline);

        m_pSetPipeline = pPipelineVK;
        vkCmdBindPipeline(m_pHandle, VK_PIPELINE_BIND_POINT_COMPUTE, pPipelineVK->m_pHandle);
    }

    void VKCommandList::SetGraphicsDescriptorSets(const std::initializer_list<DescriptorSet *> &sets)
    {
        ZoneScoped;

        u32 idx = 0;
        VkDescriptorSet pSets[LR_MAX_DESCRIPTOR_SETS_PER_PIPELINE];
        for (auto &pSet : sets)
        {
            API_VAR(VKDescriptorSet, pSet);

            pSets[idx] = pSetVK->m_pHandle;

            idx++;
        }

        vkCmdBindDescriptorSets(
            m_pHandle,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_pSetPipeline->m_pLayout,
            0,
            sets.size(),
            pSets,
            0,
            nullptr);
    }

    void VKCommandList::SetComputeDescriptorSets(const std::initializer_list<DescriptorSet *> &sets)
    {
        ZoneScoped;

        u32 idx = 0;
        VkDescriptorSet pSets[LR_MAX_DESCRIPTOR_SETS_PER_PIPELINE];
        for (auto &pSet : sets)
        {
            API_VAR(VKDescriptorSet, pSet);

            pSets[idx] = pSetVK->m_pHandle;

            idx++;
        }

        vkCmdBindDescriptorSets(
            m_pHandle,
            VK_PIPELINE_BIND_POINT_COMPUTE,
            m_pSetPipeline->m_pLayout,
            0,
            sets.size(),
            pSets,
            0,
            nullptr);
    }

    void VKCommandList::SetGraphicsPushConstants(
        Pipeline *pPipeline, ShaderStage stage, void *pData, u32 dataSize)
    {
        ZoneScoped;

        API_VAR(VKPipeline, pPipeline);

        vkCmdPushConstants(m_pHandle, pPipelineVK->m_pLayout, VKAPI::ToVKShaderType(stage), 0, dataSize, pData);
    }

    void VKCommandList::SetComputePushConstants(Pipeline *pPipeline, void *pData, u32 dataSize)
    {
        ZoneScoped;

        API_VAR(VKPipeline, pPipeline);

        vkCmdPushConstants(m_pHandle, pPipelineVK->m_pLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, dataSize, pData);
    }

}  // namespace lr::Graphics