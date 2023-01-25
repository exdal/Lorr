//
// Created on Tuesday 10th May 2022 by e-erdal
//

#pragma once

#include "Core/Graphics/RHI/Base/BaseCommandList.hh"

#include "VKSym.hh"

#include "VKPipeline.hh"
#include "VKResource.hh"

namespace lr::Graphics
{
    struct VKCommandAllocator : CommandAllocator
    {
        VkCommandPool m_pHandle = nullptr;
    };

    // A VKCommandList cannot execute itself, to execute it, we need to use VKCommandQueue
    struct VKCommandList : CommandList
    {
        void Init(VkCommandBuffer pHandle, VkFence pFence, CommandListType type);

        void BeginPass(CommandListBeginDesc *pDesc) override;
        void EndPass() override;
        void ClearImage(Image *pImage, ClearValue val) override;

        /// Memory Barriers
        void SetImageBarrier(Image *pImage, PipelineBarrier *pBarrier) override;
        void SetBufferBarrier(Buffer *pBuffer, PipelineBarrier *pBarrier) override;

        /// Buffer Commands
        void SetVertexBuffer(Buffer *pBuffer) override;
        void SetIndexBuffer(Buffer *pBuffer, bool type32 = true) override;
        void CopyBuffer(Buffer *pSource, Buffer *pDest, u32 size) override;
        void CopyBuffer(Buffer *pSource, Image *pDest) override;

        /// Draw Commands
        void Draw(u32 vertexCount, u32 firstVertex = 0, u32 instanceCount = 1, u32 firstInstance = 1) override;
        void DrawIndexed(
            u32 indexCount,
            u32 firstIndex = 0,
            u32 vertexOffset = 0,
            u32 instanceCount = 1,
            u32 firstInstance = 0) override;
        void Dispatch(u32 groupX, u32 groupY, u32 groupZ) override;

        /// Pipeline States
        void SetViewport(u32 id, u32 x, u32 y, u32 width, u32 height) override;
        void SetScissors(u32 id, u32 x, u32 y, u32 width, u32 height) override;
        void SetPrimitiveType(PrimitiveType type) override;

        void SetGraphicsPipeline(Pipeline *pPipeline) override;
        void SetComputePipeline(Pipeline *pPipeline) override;
        void SetGraphicsDescriptorSets(const std::initializer_list<DescriptorSet *> &sets) override;
        void SetComputeDescriptorSets(const std::initializer_list<DescriptorSet *> &sets) override;
        void SetGraphicsPushConstants(
            Pipeline *pPipeline, ShaderStage stage, void *pData, u32 dataSize) override;
        void SetComputePushConstants(Pipeline *pPipeline, void *pData, u32 dataSize) override;

        VKCommandAllocator *m_pAllocator = nullptr;
        VkCommandBuffer m_pHandle = nullptr;
        VkFence m_pFence = nullptr;

        VKPipeline *m_pSetPipeline = nullptr;
    };

}  // namespace lr::Graphics