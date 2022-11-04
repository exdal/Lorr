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

    void VKCommandList::SetViewport(u32 id, u32 width, u32 height, f32 minDepth, f32 maxDepth)
    {
        ZoneScoped;

        VkViewport vp;
        vp.width = width;
        vp.height = height;
        vp.x = 0;
        vp.y = 0;
        vp.minDepth = minDepth;
        vp.maxDepth = maxDepth;

        vkCmdSetViewport(m_pHandle, 0, 1, &vp);
    }

    void VKCommandList::SetScissor(u32 id, u32 x, u32 y, u32 w, u32 h)
    {
        ZoneScoped;

        VkRect2D sc;

        sc.offset.x = x;
        sc.offset.y = y;
        sc.extent.width = w;
        sc.extent.height = h;

        vkCmdSetScissor(m_pHandle, id, 1, &sc);
    }

    void VKCommandList::BarrierTransition(BaseImage *pImage,
                                          ResourceUsage barrierBefore,
                                          ShaderStage shaderBefore,
                                          ResourceUsage barrierAfter,
                                          ShaderStage shaderAfter)
    {
        ZoneScoped;

        API_VAR(VKImage, pImage);

        // TODO: Implement `VkImageSubresourceRange` fully
        VkImageSubresourceRange subresRange = {};
        subresRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        subresRange.baseArrayLayer = 0;
        subresRange.layerCount = 1;
        subresRange.baseMipLevel = pImageVK->m_UsingMip;
        subresRange.levelCount = pImageVK->m_TotalMips;

        VkImageMemoryBarrier barrierInfo = {};
        barrierInfo.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrierInfo.pNext = nullptr;

        barrierInfo.image = pImageVK->m_pHandle;

        barrierInfo.oldLayout = VKAPI::ToVKImageLayout(barrierBefore);
        barrierInfo.newLayout = VKAPI::ToVKImageLayout(barrierAfter);

        barrierInfo.srcAccessMask = VKAPI::ToVKAccessFlags(barrierBefore);
        barrierInfo.dstAccessMask = VKAPI::ToVKAccessFlags(barrierAfter);

        barrierInfo.subresourceRange = subresRange;

        barrierInfo.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrierInfo.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

        vkCmdPipelineBarrier(m_pHandle,
                             VKAPI::ToVKPipelineStage(barrierBefore) | VKAPI::ToVKPipelineShaderStage(shaderBefore),
                             VKAPI::ToVKPipelineStage(barrierAfter) | VKAPI::ToVKPipelineShaderStage(shaderAfter),
                             VK_DEPENDENCY_BY_REGION_BIT,
                             0,
                             nullptr,
                             0,
                             nullptr,
                             1,
                             &barrierInfo);
    }

    void VKCommandList::BarrierTransition(BaseBuffer *pBuffer,
                                          ResourceUsage barrierBefore,
                                          ShaderStage shaderBefore,
                                          ResourceUsage barrierAfter,
                                          ShaderStage shaderAfter)
    {
        ZoneScoped;

        API_VAR(VKBuffer, pBuffer);

        VkBufferMemoryBarrier barrierInfo = {};
        barrierInfo.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        barrierInfo.pNext = nullptr;

        barrierInfo.buffer = pBufferVK->m_pHandle;

        barrierInfo.offset = pBuffer->m_DataOffset;
        barrierInfo.size = VK_WHOLE_SIZE;

        barrierInfo.srcAccessMask = VKAPI::ToVKAccessFlags(barrierBefore);
        barrierInfo.dstAccessMask = VKAPI::ToVKAccessFlags(barrierAfter);

        barrierInfo.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrierInfo.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

        vkCmdPipelineBarrier(m_pHandle,
                             VKAPI::ToVKPipelineStage(barrierBefore) | VKAPI::ToVKPipelineShaderStage(shaderBefore),
                             VKAPI::ToVKPipelineStage(barrierAfter) | VKAPI::ToVKPipelineShaderStage(shaderAfter),
                             VK_DEPENDENCY_BY_REGION_BIT,
                             0,
                             nullptr,
                             1,
                             &barrierInfo,
                             0,
                             nullptr);
    }

    void VKCommandList::ClearImage(BaseImage *pImage, ClearValue val)
    {
        ZoneScoped;

        API_VAR(VKImage, pImage);

        VkClearValue color;
        memcpy(color.color.float32, &val.RenderTargetColor.x, sizeof(XMFLOAT4));

        VkImageSubresourceRange subresRange = {};
        subresRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        subresRange.baseArrayLayer = 0;
        subresRange.layerCount = 1;
        subresRange.baseMipLevel = pImageVK->m_UsingMip;
        subresRange.levelCount = pImageVK->m_TotalMips;

        vkCmdClearColorImage(m_pHandle,
                             pImageVK->m_pHandle,
                             VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                             (const VkClearColorValue *)&color,
                             1,
                             &subresRange);
    }

    void VKCommandList::SetVertexBuffer(BaseBuffer *pBuffer)
    {
        ZoneScoped;

        API_VAR(VKBuffer, pBuffer);

        VkDeviceSize pOffsets[1] = { 0 };
        vkCmdBindVertexBuffers(m_pHandle, 0, 1, &pBufferVK->m_pHandle, pOffsets);
    }

    void VKCommandList::SetIndexBuffer(BaseBuffer *pBuffer, bool type32)
    {
        ZoneScoped;

        API_VAR(VKBuffer, pBuffer);

        vkCmdBindIndexBuffer(m_pHandle, pBufferVK->m_pHandle, 0, type32 ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16);
    }

    void VKCommandList::CopyBuffer(BaseBuffer *pSource, BaseBuffer *pDest, u32 size)
    {
        ZoneScoped;

        API_VAR(VKBuffer, pSource);
        API_VAR(VKBuffer, pDest);

        VkBufferCopy copyRegion = {};
        copyRegion.size = size;

        vkCmdCopyBuffer(m_pHandle, pSourceVK->m_pHandle, pDestVK->m_pHandle, 1, &copyRegion);
    }

    void VKCommandList::CopyBuffer(BaseBuffer *pSource, BaseImage *pDest)
    {
        ZoneScoped;

        API_VAR(VKBuffer, pSource);
        API_VAR(VKImage, pDest);

        VkImageSubresourceRange subresRange = {};
        subresRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        subresRange.baseArrayLayer = 0;
        subresRange.layerCount = 1;
        subresRange.baseMipLevel = pDest->m_UsingMip;
        subresRange.levelCount = pDest->m_TotalMips;

        VkImageMemoryBarrier imageBarrier = {};
        imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        imageBarrier.pNext = nullptr;

        imageBarrier.image = pDestVK->m_pHandle;

        imageBarrier.srcAccessMask = 0;
        imageBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        imageBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

        imageBarrier.subresourceRange = subresRange;

        imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

        vkCmdPipelineBarrier(m_pHandle,
                             VK_PIPELINE_STAGE_HOST_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_DEPENDENCY_BY_REGION_BIT,
                             0,
                             nullptr,
                             0,
                             nullptr,
                             1,
                             &imageBarrier);

        // TODO: Multiple mip levels
        VkBufferImageCopy imageCopyInfo = {};
        imageCopyInfo.bufferOffset = 0;

        imageCopyInfo.imageOffset.x = 0;
        imageCopyInfo.imageOffset.y = 0;
        imageCopyInfo.imageOffset.z = 0;

        imageCopyInfo.imageExtent.width = pDest->m_Width;
        imageCopyInfo.imageExtent.height = pDest->m_Height;
        imageCopyInfo.imageExtent.depth = 1;

        imageCopyInfo.imageSubresource.aspectMask = subresRange.aspectMask;
        imageCopyInfo.imageSubresource.baseArrayLayer = 0;
        imageCopyInfo.imageSubresource.layerCount = 1;
        imageCopyInfo.imageSubresource.mipLevel = 0;

        vkCmdCopyBufferToImage(m_pHandle, pSourceVK->m_pHandle, pDestVK->m_pHandle, imageBarrier.newLayout, 1, &imageCopyInfo);

        imageBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        imageBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        imageBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        imageBarrier.newLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;

        vkCmdPipelineBarrier(m_pHandle,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                             VK_DEPENDENCY_BY_REGION_BIT,
                             0,
                             nullptr,
                             0,
                             nullptr,
                             1,
                             &imageBarrier);

        pDestVK->m_FinalLayout = imageBarrier.newLayout;
    }

    void VKCommandList::Draw(u32 vertexCount, u32 firstVertex, u32 instanceCount, u32 firstInstance)
    {
        ZoneScoped;

        vkCmdDraw(m_pHandle, vertexCount, instanceCount, firstVertex, firstInstance);
    }

    void VKCommandList::DrawIndexed(u32 indexCount, u32 firstIndex, u32 vertexOffset, u32 instanceCount, u32 firstInstance)
    {
        ZoneScoped;

        vkCmdDrawIndexed(m_pHandle, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
    }

    void VKCommandList::SetPipeline(BasePipeline *pPipeline)
    {
        ZoneScoped;

        API_VAR(VKPipeline, pPipeline);

        // TODO: Support multiple bind points
        vkCmdBindPipeline(m_pHandle, VK_PIPELINE_BIND_POINT_GRAPHICS, pPipelineVK->pHandle);

        m_pSetPipeline = pPipelineVK;
    }

    void VKCommandList::SetPipelineDescriptorSets(const std::initializer_list<BaseDescriptorSet *> &sets)
    {
        ZoneScoped;

        u32 idx = 0;
        VkDescriptorSet pSets[8];
        for (auto &pSet : sets)
        {
            API_VAR(VKDescriptorSet, pSet);

            pSets[idx] = pSetVK->pHandle;

            idx++;
        }

        vkCmdBindDescriptorSets(m_pHandle, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pSetPipeline->pLayout, 0, sets.size(), pSets, 0, nullptr);
    }

}  // namespace lr::Graphics