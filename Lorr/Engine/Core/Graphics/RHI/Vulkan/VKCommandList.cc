#include "VKCommandList.hh"

#include "Engine.hh"

#include "VKAPI.hh"
#include "VKResource.hh"

namespace lr::Graphics
{
    void VKCommandList::Init(VkCommandBuffer pHandle, CommandListType type, VkFence pFence)
    {
        ZoneScoped;

        m_pHandle = pHandle;
        m_Type = type;
        m_pFence = pFence;
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

    void VKCommandList::SetVertexBuffer(VKBuffer *pBuffer)
    {
        ZoneScoped;

        VkDeviceSize pOffsets[1] = { 0 };
        vkCmdBindVertexBuffers(m_pHandle, 0, 1, &pBuffer->m_pHandle, pOffsets);
    }

    void VKCommandList::SetIndexBuffer(VKBuffer *pBuffer, bool type32)
    {
        ZoneScoped;

        vkCmdBindIndexBuffer(m_pHandle, pBuffer->m_pHandle, 0, type32 ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16);
    }

    void VKCommandList::CopyBuffer(VKBuffer *pSource, VKBuffer *pDest, u32 size)
    {
        ZoneScoped;

        VkBufferCopy copyRegion = {};
        copyRegion.size = size;

        vkCmdCopyBuffer(m_pHandle, pSource->m_pHandle, pDest->m_pHandle, 1, &copyRegion);
    }

    void VKCommandList::CopyBuffer(VKBuffer *pSource, VKImage *pDest)
    {
        ZoneScoped;

        VkImageSubresourceRange subresRange = {};
        subresRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        subresRange.baseArrayLayer = 0;
        subresRange.layerCount = 1;
        subresRange.baseMipLevel = pDest->m_UsingMip;
        subresRange.levelCount = pDest->m_TotalMips;

        VkImageMemoryBarrier imageBarrier = {};
        imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        imageBarrier.pNext = nullptr;

        imageBarrier.image = pDest->m_pHandle;

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

        vkCmdCopyBufferToImage(m_pHandle, pSource->m_pHandle, pDest->m_pHandle, imageBarrier.newLayout, 1, &imageCopyInfo);

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

        pDest->m_FinalLayout = imageBarrier.newLayout;
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

    void VKCommandList::BeginRenderPass(CommandRenderPassBeginInfo &beginInfo, VkRenderPass pRenderPass, VkFramebuffer pFrameBuffer)
    {
        ZoneScoped;

        VKAPI *pAPI = GetEngine()->m_pAPI;
        APIStateManager &stateMan = pAPI->m_APIStateMan;
        VKSwapChain &swapChain = pAPI->m_SwapChain;
        VKSwapChainFrame *pCurrentFrame = swapChain.GetCurrentFrame();

        // TODO: Clear colors per attachemtns
        VkClearValue pClearValues[8];

        VkExtent2D renderAreaExt = { beginInfo.RenderArea.z, beginInfo.RenderArea.w };

        // Get info from SwapChain
        if (beginInfo.RenderArea.z == -1 && beginInfo.RenderArea.w == -1)
        {
            renderAreaExt.width = swapChain.m_Width;
            renderAreaExt.height = swapChain.m_Height;
        }

        // Set clear colors
        for (u32 i = 0; i < beginInfo.ClearValueCount; i++)
        {
            ClearValue &currentClearColor = beginInfo.pClearValues[i];

            if (!currentClearColor.IsDepth)
            {
                memcpy(pClearValues[i].color.float32, &currentClearColor.RenderTargetColor, sizeof(XMFLOAT4));
            }
            else
            {
                pClearValues[i].depthStencil.depth = currentClearColor.DepthStencilColor.Depth;
                pClearValues[i].depthStencil.stencil = currentClearColor.DepthStencilColor.Stencil;
            }
        }

        // Actual info
        VkRenderPassBeginInfo renderPassInfo = {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = pRenderPass;
        renderPassInfo.framebuffer = pFrameBuffer;

        renderPassInfo.renderArea.offset.x = beginInfo.RenderArea.x;
        renderPassInfo.renderArea.offset.y = beginInfo.RenderArea.y;

        renderPassInfo.renderArea.extent = renderAreaExt;

        renderPassInfo.clearValueCount = beginInfo.ClearValueCount;
        renderPassInfo.pClearValues = pClearValues;

        vkCmdBeginRenderPass(m_pHandle, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    }

    void VKCommandList::EndRenderPass()
    {
        ZoneScoped;

        vkCmdEndRenderPass(m_pHandle);
    }

    void VKCommandList::SetPipeline(VKPipeline *pPipeline)
    {
        ZoneScoped;

        // TODO: Support multiple bind points
        vkCmdBindPipeline(m_pHandle, VK_PIPELINE_BIND_POINT_GRAPHICS, pPipeline->pHandle);

        m_pSetPipeline = pPipeline;
    }

    void VKCommandList::SetPipelineDescriptorSets(const std::initializer_list<VKDescriptorSet *> &sets)
    {
        ZoneScoped;

        u32 idx = 0;
        VkDescriptorSet pSets[8];
        for (auto &pSet : sets)
        {
            pSets[idx] = pSet->pHandle;

            idx++;
        }

        vkCmdBindDescriptorSets(m_pHandle, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pSetPipeline->pLayout, 0, sets.size(), pSets, 0, nullptr);
    }

}  // namespace lr::Graphics