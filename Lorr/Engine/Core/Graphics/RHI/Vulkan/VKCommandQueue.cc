#include "VKCommandQueue.hh"

#include "VKCommandList.hh"

namespace lr::Graphics
{
    void VKCommandQueue::Init(VkQueue pHandle)
    {
        ZoneScoped;

        m_pHandle = pHandle;
    }

    void VKCommandQueue::SetSemaphoreStage(VkPipelineStageFlags2 acquire, VkPipelineStageFlags2 present)
    {
        ZoneScoped;

        m_AcquireStage = acquire;
        m_PresentStage = present;
    }

}  // namespace lr::Graphics