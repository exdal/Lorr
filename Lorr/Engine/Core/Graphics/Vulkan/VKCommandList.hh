//
// Created on Tuesday 10th May 2022 by exdal
//

#pragma once

#include "Allocator.hh"

#include "VKCommandQueue.hh"
#include "VKPipeline.hh"
#include "VKResource.hh"

#include "Vulkan.hh"

namespace lr::Graphics
{
    enum CommandListType : u32
    {
        LR_COMMAND_LIST_TYPE_GRAPHICS = 0,
        LR_COMMAND_LIST_TYPE_COMPUTE,
        LR_COMMAND_LIST_TYPE_TRANSFER,
        LR_COMMAND_LIST_TYPE_COUNT,
    };

    enum PipelineStage : u32
    {
        LR_PIPELINE_STAGE_NONE = 0,
        LR_PIPELINE_STAGE_VERTEX_INPUT = 1 << 0,
        LR_PIPELINE_STAGE_VERTEX_SHADER = 1 << 1,
        LR_PIPELINE_STAGE_PIXEL_SHADER = 1 << 2,
        LR_PIPELINE_STAGE_EARLY_PIXEL_TESTS = 1 << 3,
        LR_PIPELINE_STAGE_LATE_PIXEL_TESTS = 1 << 4,
        LR_PIPELINE_STAGE_RENDER_TARGET = 1 << 5,
        LR_PIPELINE_STAGE_COMPUTE_SHADER = 1 << 6,
        LR_PIPELINE_STAGE_TRANSFER = 1 << 7,
        LR_PIPELINE_STAGE_ALL_COMMANDS = 1 << 8,
        LR_PIPELINE_STAGE_HOST = 1 << 9,
    };
    EnumFlags(PipelineStage);

    struct RenderPassBeginDesc
    {
        XMUINT4 m_RenderArea = { 0, 0, UINT32_MAX, UINT32_MAX };

        eastl::span< ColorAttachment> m_ColorAttachments;
        DepthAttachment *m_pDepthAttachment = nullptr;
    };

    struct PipelineBarrier
    {
        union
        {
            u32 __u_data = 0;
            struct
            {
                ImageLayout m_CurrentLayout : 16;
                ImageLayout m_NextLayout : 16;
            };
        };

        PipelineStage m_CurrentStage = LR_PIPELINE_STAGE_NONE;
        PipelineStage m_NextStage = LR_PIPELINE_STAGE_NONE;
        MemoryAcces m_CurrentAccess = LR_MEMORY_ACCESS_NONE;
        MemoryAcces m_NextAccess = LR_MEMORY_ACCESS_NONE;
        CommandListType m_CurrentQueue = LR_COMMAND_LIST_TYPE_COUNT;
        CommandListType m_NextQueue = LR_COMMAND_LIST_TYPE_COUNT;
    };

    struct Fence : APIObject<VK_OBJECT_TYPE_FENCE>
    {
        VkFence m_pHandle = VK_NULL_HANDLE;
    };

    struct Semaphore : APIObject<VK_OBJECT_TYPE_SEMAPHORE>
    {
        VkSemaphore m_pHandle = VK_NULL_HANDLE;
        u64 m_Value = 0;
    };

    struct SemaphoreSubmitDesc : VkSemaphoreSubmitInfo
    {
        SemaphoreSubmitDesc(Semaphore *pSemaphore, u64 value, PipelineStage stage);
        SemaphoreSubmitDesc(Semaphore *pSemaphore, u64 value);
        SemaphoreSubmitDesc() = default;
    };

    struct CommandAllocator : APIObject<VK_OBJECT_TYPE_COMMAND_POOL>
    {
        VkCommandPool m_pHandle = VK_NULL_HANDLE;
    };

    struct CommandList : APIObject<VK_OBJECT_TYPE_COMMAND_BUFFER>
    {
        void BeginPass(RenderPassBeginDesc *pDesc);
        void EndPass();

        /// Memory Barriers
        void SetMemoryBarrier(PipelineBarrier *pBarrier);
        void SetImageBarrier(Image *pImage, PipelineBarrier *pBarrier);
        void SetBufferBarrier(Buffer *pBuffer, PipelineBarrier *pBarrier);

        /// Buffer Commands
        void SetVertexBuffer(Buffer *pBuffer);
        void SetIndexBuffer(Buffer *pBuffer, bool type32 = true);
        void CopyBuffer(Buffer *pSource, Buffer *pDest, u32 size);
        void CopyBuffer(Buffer *pSource, Image *pDest);

        /// Draw Commands
        void Draw(u32 vertexCount, u32 firstVertex = 0, u32 instanceCount = 1, u32 firstInstance = 1);
        void DrawIndexed(
            u32 indexCount,
            u32 firstIndex = 0,
            u32 vertexOffset = 0,
            u32 instanceCount = 1,
            u32 firstInstance = 0);
        void Dispatch(u32 groupX, u32 groupY, u32 groupZ);

        /// Pipeline States
        void SetViewport(u32 id, u32 x, u32 y, u32 width, u32 height);
        void SetScissors(u32 id, u32 x, u32 y, u32 width, u32 height);
        void SetPrimitiveType(PrimitiveType type);

        void SetPipeline(Pipeline *pPipeline);
        void SetDescriptorSets(const std::initializer_list<DescriptorSet *> &sets);
        void SetPushConstants(ShaderStage stage, u32 offset, void *pData, u32 dataSize);

        CommandListType m_Type = LR_COMMAND_LIST_TYPE_COUNT;
        VkCommandBuffer m_pHandle = VK_NULL_HANDLE;
        CommandAllocator *m_pAllocator = nullptr;

        Pipeline *m_pPipeline = nullptr;

        AvailableQueueInfo *m_pQueueInfo = nullptr;
    };

    struct CommandListSubmitDesc : VkCommandBufferSubmitInfo
    {
        CommandListSubmitDesc(CommandList *pList);
    };

}  // namespace lr::Graphics