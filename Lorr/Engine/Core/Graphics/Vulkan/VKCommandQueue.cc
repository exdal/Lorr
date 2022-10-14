#include "VKCommandQueue.hh"

#include "VKCommandList.hh"

namespace lr::Graphics
{
    void VKCommandQueue::Init(VkQueue pHandle, VkFence pBatchFence)
    {
        ZoneScoped;

        m_pHandle = pHandle;
        m_pBatchFence = pBatchFence;
    }

    bool VKCommandQueue::AddList(VKCommandList *pList)
    {
        ZoneScoped;

        if (m_BatchedListsCount == APIConfig::kMaxCommandListPerBatch)
        {
            LOG_TRACE("Trying to push a list into a full batch list.");

            return false;
        }

        m_BatchedLists[m_BatchedListsCount] = pList;
        m_BatchedListHandles[m_BatchedListsCount] = pList->m_pHandle;
        m_BatchedListsCount++;

        return true;
    }

    void VKCommandQueue::SetSemaphoreStage(VkPipelineStageFlags2 acquire, VkPipelineStageFlags2 present)
    {
        ZoneScoped;

        m_AcquireStage = acquire;
        m_PresentStage = present;
    }

}  // namespace lr::Graphics