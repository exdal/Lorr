//
// Created on Sunday 22nd May 2022 by e-erdal
//

#pragma once

#include "Core/Graphics/APIConfig.hh"

#include "VKSym.hh"
#include "VKCommandList.hh"

namespace lr::Graphics
{
    struct VKCommandQueue
    {
        void Init(VkQueue pHandle, VkFence pBatchFence);

        // Adds a command list to batched queue, cannot remove after adding
        bool AddList(VKCommandList *pList);
        void SetSemaphoreStage(VkPipelineStageFlags2 acquire, VkPipelineStageFlags2 present);

        VkQueue m_pHandle = nullptr;

        bool m_IsHighPriortiy = false;
        CommandListType m_Type;

        /// Batching

        VkPipelineStageFlags2 m_AcquireStage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkPipelineStageFlags2 m_PresentStage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

        VkFence m_pBatchFence = nullptr;

        eastl::array<VKCommandList *, APIConfig::kMaxCommandListPerBatch> m_BatchedLists;
        eastl::array<VkCommandBuffer, APIConfig::kMaxCommandListPerBatch> m_BatchedListHandles;
        u32 m_BatchedListsCount = 0;
    };

}  // namespace lr::Graphics