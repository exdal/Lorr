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
    struct VKCommandAllocator : BaseCommandAllocator
    {
        VkCommandPool pHandle = nullptr;
    };

    // A VKCommandList cannot execute itself, to execute it, we need to use VKCommandQueue
    struct VKCommandList : BaseCommandList
    {
        void Init(VkCommandBuffer pHandle, VkFence pFence, CommandListType type);

        void BeginPass(CommandListBeginDesc *pDesc) override;
        void EndPass() override;

        void BarrierTransition(BaseImage *pImage,
                               ResourceUsage barrierBefore,
                               ShaderStage shaderBefore,
                               ResourceUsage barrierAfter,
                               ShaderStage shaderAfter) override;

        void BarrierTransition(BaseBuffer *pBuffer,
                               ResourceUsage barrierBefore,
                               ShaderStage shaderBefore,
                               ResourceUsage barrierAfter,
                               ShaderStage shaderAfter) override;

        void ClearImage(BaseImage *pImage, ClearValue val) override;

        void SetViewport(u32 id, u32 x, u32 y, u32 width, u32 height) override;
        void SetScissors(u32 id, u32 x, u32 y, u32 width, u32 height) override;
        void SetPrimitiveType(PrimitiveType type) override;

        /// Buffer Commands
        void SetPushConstants(BasePipeline *pPipeline, ShaderStage stage, void *pData, u32 dataSize) override;
        void SetVertexBuffer(BaseBuffer *pBuffer) override;
        void SetIndexBuffer(BaseBuffer *pBuffer, bool type32) override;
        void CopyBuffer(BaseBuffer *pSource, BaseBuffer *pDest, u32 size) override;
        void CopyBuffer(BaseBuffer *pSource, BaseImage *pDest) override;

        /// Draw Commands
        void Draw(u32 vertexCount, u32 firstVertex, u32 instanceCount, u32 firstInstance) override;
        void DrawIndexed(u32 indexCount, u32 firstIndex, u32 vertexOffset, u32 instanceCount, u32 firstInstance) override;

        // Pipeline
        void SetPipeline(BasePipeline *pPipeline) override;
        void SetPipelineDescriptorSets(const std::initializer_list<BaseDescriptorSet *> &sets) override;

        VKCommandAllocator *m_pAllocator = nullptr;
        VkCommandBuffer m_pHandle = nullptr;
        VkFence m_pFence = nullptr;

        VKPipeline *m_pSetPipeline = nullptr;
    };

}  // namespace lr::Graphics