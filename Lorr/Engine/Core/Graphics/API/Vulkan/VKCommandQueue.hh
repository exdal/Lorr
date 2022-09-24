//
// Created on Sunday 22nd May 2022 by e-erdal
//

#pragma once

#include "VKSym.hh"
#include "VKCommandList.hh"

namespace lr::g
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

        static constexpr u32 kMaxCommandListPerBatch = 8;
        eastl::array<VKCommandList *, kMaxCommandListPerBatch> m_BatchedLists;
        eastl::array<VkCommandBuffer, kMaxCommandListPerBatch> m_BatchedListHandles;
        u32 m_BatchedListsCount = 0;
    };

}  // namespace lr::g