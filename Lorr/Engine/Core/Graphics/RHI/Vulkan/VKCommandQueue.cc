#include "VKCommandQueue.hh"

#include "VKCommandList.hh"

namespace lr::Graphics
{
    void VKCommandQueue::Init(VkQueue pHandle)
    {
        ZoneScoped;

        m_pHandle = pHandle;
    }

}  // namespace lr::Graphics