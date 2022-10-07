#include "VKCommandList.hh"

#include "Engine.hh"

#include "VKAPI.hh"
#include "VKResource.hh"

namespace lr::g
{
    void VKCommandList::Init(VkCommandBuffer pHandle, CommandListType type, VkFence pFence)
    {
        ZoneScoped;

        m_pHandle = pHandle;
        m_Type = type;
        m_pFence = pFence;
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
        VkClearValue clearColor = { 0.0, 0.0, 0.0, 1.0 };
        VkClearValue depthColor = {};

        VkExtent2D renderAreaExt = { beginInfo.RenderArea.z, beginInfo.RenderArea.w };

        // Get info from SwapChain
        if (!pRenderPass)
        {
            pRenderPass = swapChain.m_pRenderPass;
            pFrameBuffer = pCurrentFrame->pFrameBuffer;
            renderAreaExt.width = swapChain.m_Width;
            renderAreaExt.height = swapChain.m_Height;
        }

        // Set clear colors
        memcpy(clearColor.color.float32, &beginInfo.pClearValues[0].RenderTargetColor, sizeof(XMFLOAT4));
        depthColor.depthStencil.depth = beginInfo.pClearValues[1].DepthStencil.Depth;
        depthColor.depthStencil.stencil = beginInfo.pClearValues[1].DepthStencil.Stencil;

        VkClearValue pClearValues[2] = { clearColor, depthColor };

        // Actual info
        VkRenderPassBeginInfo renderPassInfo = {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = pRenderPass;
        renderPassInfo.framebuffer = pFrameBuffer;

        renderPassInfo.renderArea.offset.x = beginInfo.RenderArea.x;
        renderPassInfo.renderArea.offset.y = beginInfo.RenderArea.y;

        renderPassInfo.renderArea.extent = renderAreaExt;

        renderPassInfo.clearValueCount = 2;
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
            // Load mask
            auto mask = maskAtomic.load(eastl::memory_order_acquire);

            // Get available bit and return
            setBitPosition = log2(mask & -mask);

            // Set that bit to 0 and store, so we make it "in use"
            auto newMask = mask ^ (1 << setBitPosition);

            // Additional checks
            if (mask == 0)
            {
                LOG_TRACE("All lists are in use, we have to wait for one to become available.");
                return;
            }

            while (!maskAtomic.compare_exchange_weak(mask, newMask, eastl::memory_order_release))
                ;

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

                    while (!maskAtomic.compare_exchange_weak(mask, newMask, eastl::memory_order_release))
                        ;

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

}  // namespace lr::g