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

    void VKCommandList::BeginRenderPass(CommandRenderPassBeginInfo &beginInfo, VkRenderPass pRenderPass)
    {
        ZoneScoped;

        Engine *pEngine = GetEngine();
        VKSwapChain &swapChain = pEngine->m_pAPI->m_SwapChain;
        SwapChainFrame *pCurrentFrame = swapChain.GetCurrentFrame();

        VkFramebuffer pFrameBuffer = nullptr;

        // TODO: Clear colors per attachemtns
        VkClearValue pClearValues[8];

        VkExtent2D renderAreaExt = { beginInfo.RenderArea.z, beginInfo.RenderArea.w };

        // Get info from SwapChain
        if (!pRenderPass)
        {
            pRenderPass = swapChain.m_pRenderPass;
            renderAreaExt.width = swapChain.m_Width;
            renderAreaExt.height = swapChain.m_Height;
        }

        if (!pFrameBuffer)
            pFrameBuffer = pCurrentFrame->pFrameBuffer;

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
        }

        vkCmdBindDescriptorSets(m_pHandle, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pSetPipeline->pLayout, 0, sets.size(), pSets, 0, nullptr);
    }

    void CommandListPool::Init()
    {
        ZoneScoped;

        m_DirectListMask.store(UINT32_MAX, eastl::memory_order_release);
        m_ComputeListMask.store(UINT16_MAX, eastl::memory_order_release);
        m_CopyListMask.store(UINT8_MAX, eastl::memory_order_release);
    }

    VKCommandList *CommandListPool::Request(CommandListType type)
    {
        ZoneScoped;

        VKCommandList *pList = nullptr;
        u32 setBitPosition = 0;

        auto GetListFromMask = [&](auto &&maskAtomic, auto &&listArray) {
            auto mask = maskAtomic.load(eastl::memory_order_acquire);

            if (mask == 0)
            {
                // LOG_TRACE("All lists are in use, we have to wait for one to become available.");

                while (!mask)
                {
                    mask = maskAtomic.load(eastl::memory_order_acquire);
                }
            }

            setBitPosition = Memory::GetLSB(mask);
            auto newMask = mask ^ (1 << setBitPosition);
            maskAtomic.store(newMask, eastl::memory_order_release);

            // while (!maskAtomic.compare_exchange_weak(mask, newMask, eastl::memory_order_release))
            //     ;

            pList = &listArray[setBitPosition];
        };

        switch (type)
        {
            case CommandListType::Direct: GetListFromMask(m_DirectListMask, m_DirectLists); break;
            case CommandListType::Compute: GetListFromMask(m_ComputeListMask, m_ComputeLists); break;
            case CommandListType::Copy: GetListFromMask(m_CopyListMask, m_CopyLists); break;

            default: break;
        }

        return pList;
    }

    void CommandListPool::SetExecuteState(VKCommandList *pList)
    {
        ZoneScoped;

        for (u32 i = 0; i < m_DirectLists.count; i++)
        {
            if (&m_DirectLists[i] == pList)
            {
                u32 fenceMask = m_DirectFenceMask.load(eastl::memory_order_acquire);
                fenceMask |= 1 << Memory::GetLSB(fenceMask);
                m_DirectFenceMask.store(fenceMask, eastl::memory_order_release);
            }
        }
    }

    void CommandListPool::Discard(VKCommandList *pList)
    {
        ZoneScoped;

        auto PlaceIntoArray = [&](auto &&maskAtomic, auto &&listArray) {
            for (u32 i = 0; i < listArray.count; i++)
            {
                if (&listArray[i] == pList)
                {
                    // We really don't care if the bit is set before, it shouldn't happen anyway

                    auto mask = maskAtomic.load(eastl::memory_order_acquire);
                    auto newMask = mask | (1 << i);
                    maskAtomic.store(newMask, eastl::memory_order_release);

                    // while (!maskAtomic.compare_exchange_weak(mask, newMask, eastl::memory_order_release))
                    //     ;

                    return;
                }
            }

            LOG_ERROR("Trying to push unknown Command List into the Command List pool.");
        };

        switch (pList->m_Type)
        {
            case CommandListType::Direct: PlaceIntoArray(m_DirectListMask, m_DirectLists); break;
            case CommandListType::Compute: PlaceIntoArray(m_ComputeListMask, m_ComputeLists); break;
            case CommandListType::Copy: PlaceIntoArray(m_CopyListMask, m_CopyLists); break;

            default: break;
        }
    }

    void CommandListPool::WaitForAll()
    {
        ZoneScoped;

        u32 mask = m_DirectListMask.load(eastl::memory_order_acquire);

        while (mask != UINT32_MAX)
        {
            EAProcessorPause();
        }
    }

}  // namespace lr::Graphics