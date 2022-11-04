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

        void SetViewport(u32 id, u32 width, u32 height, f32 minDepth, f32 maxDepth);
        void SetScissor(u32 id, u32 x, u32 y, u32 w, u32 h);

        void BarrierTransition(BaseImage *pImage,
                               ResourceUsage barrierBefore,
                               ShaderStage shaderBefore,
                               ResourceUsage barrierAfter,
                               ShaderStage shaderAfter);
                               
        void BarrierTransition(BaseBuffer *pBuffer,
                               ResourceUsage barrierBefore,
                               ShaderStage shaderBefore,
                               ResourceUsage barrierAfter,
                               ShaderStage shaderAfter);

        void ClearImage(BaseImage *pImage, ClearValue val);

        /// Buffer Commands
        void SetVertexBuffer(BaseBuffer *pBuffer);
        void SetIndexBuffer(BaseBuffer *pBuffer, bool type32);
        void CopyBuffer(BaseBuffer *pSource, BaseBuffer *pDest, u32 size);
        void CopyBuffer(BaseBuffer *pSource, BaseImage *pDest);

        /// Draw Commands
        void Draw(u32 vertexCount, u32 firstVertex, u32 instanceCount, u32 firstInstance);
        void DrawIndexed(u32 indexCount, u32 firstIndex, u32 vertexOffset, u32 instanceCount, u32 firstInstance);

        // Pipeline
        void SetPipeline(BasePipeline *pPipeline);
        void SetPipelineDescriptorSets(const std::initializer_list<BaseDescriptorSet *> &sets);

        VKCommandAllocator *m_pAllocator = nullptr;
        VkCommandBuffer m_pHandle = nullptr;
        VkFence m_pFence = nullptr;

        VKPipeline *m_pSetPipeline = nullptr;
    };

}  // namespace lr::Graphics