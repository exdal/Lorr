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
        void Init(VkCommandBuffer pHandle, CommandListType type, VkFence pFence);

        void SetViewport(u32 id, u32 width, u32 height, f32 minDepth, f32 maxDepth);
        void SetScissor(u32 id, u32 x, u32 y, u32 w, u32 h);

        /// Buffer Commands
        void SetVertexBuffer(VKBuffer *pBuffer);
        void SetIndexBuffer(VKBuffer *pBuffer, bool type32 = true);
        void CopyBuffer(VKBuffer *pSource, VKBuffer *pDest, u32 size);
        void CopyBuffer(VKBuffer *pSource, VKImage *pDest);

        /// Draw Commands
        void Draw(u32 vertexCount, u32 firstVertex = 0, u32 instanceCount = 1, u32 firstInstance = 1);
        void DrawIndexed(u32 indexCount, u32 firstIndex = 0, u32 vertexOffset = 0, u32 instanceCount = 1, u32 firstInstance = 1);

        /// RenderPass Commands

        // If pRenderPass is a nullptr, we use SwapChain's renderpass
        void BeginRenderPass(CommandRenderPassBeginInfo &beginInfo, VkRenderPass pRenderPass, VkFramebuffer pFrameBuffer);
        void EndRenderPass();

        // Pipeline
        void SetPipeline(VKPipeline *pPipeline);
        void SetPipelineDescriptorSets(const std::initializer_list<VKDescriptorSet *> &sets);

        VkCommandBuffer m_pHandle = nullptr;
        VkFence m_pFence = nullptr;

        VKPipeline *m_pSetPipeline = nullptr;
    };

}  // namespace lr::Graphics