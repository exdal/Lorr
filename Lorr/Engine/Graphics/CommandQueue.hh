//
// Created on Sunday 22nd May 2022 by exdal
//

#pragma once

#include "APIAllocator.hh"

namespace lr::Graphics
{
struct AvailableQueueInfo
{
    u32 m_Present : 1;
    u32 m_QueueCount : 15;
    u32 m_Index : 16;
};

struct CommandQueue : APIObject<VK_OBJECT_TYPE_QUEUE>
{
    VkQueue m_pHandle = nullptr;
};

}  // namespace lr::Graphics