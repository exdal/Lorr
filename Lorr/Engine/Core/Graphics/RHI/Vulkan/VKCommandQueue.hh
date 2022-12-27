//
// Created on Sunday 22nd May 2022 by e-erdal
//

#pragma once

#include "Core/Graphics/RHI/APIConfig.hh"
#include "Core/Graphics/RHI/Base/BaseCommandQueue.hh"

#include "VKSym.hh"
#include "VKCommandList.hh"

namespace lr::Graphics
{
    struct VKCommandQueue : BaseCommandQueue
    {
        void Init(VkQueue pHandle);
        void SetSemaphoreStage(VkPipelineStageFlags2 acquire, VkPipelineStageFlags2 present);

        VkQueue m_pHandle = nullptr;

        bool m_IsHighPriortiy = false;
        CommandListType m_Type;

        VkPipelineStageFlags2 m_AcquireStage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkPipelineStageFlags2 m_PresentStage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    };

}  // namespace lr::Graphics