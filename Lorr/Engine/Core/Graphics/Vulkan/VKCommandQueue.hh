//
// Created on Sunday 22nd May 2022 by exdal
//

#pragma once

#include "Allocator.hh"
#include "Vulkan.hh"

namespace lr::Graphics
{
    struct AvailableQueueInfo
    {
        u32 m_QueueCount = 0;
        eastl::array<u8, 4> m_Indexes;
    };

    struct CommandQueue : APIObject<VK_OBJECT_TYPE_QUEUE>
    {
        void Init(VkQueue pHandle)
        {
            m_pHandle = pHandle;
        }

        VkQueue m_pHandle = nullptr;
    };

}  // namespace lr::Graphics