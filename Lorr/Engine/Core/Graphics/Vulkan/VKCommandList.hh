//
// Created on Tuesday 10th May 2022 by e-erdal
//

#pragma once

#include "Core/Graphics/APIConfig.hh"

#include "VKSym.hh"

#include "VKPipeline.hh"
#include "VKResource.hh"

namespace lr::Graphics
{
    struct ClearValue
    {
        ClearValue()
        {
        }

        ClearValue(const XMFLOAT4 &color)
        {
            IsDepth = false;

            RenderTargetColor = color;
        }

        ClearValue(f32 depth, u8 stencil)
        {
            IsDepth = true;

            DepthStencilColor.Depth = depth;
            DepthStencilColor.Stencil = stencil;
        }

        XMFLOAT4 RenderTargetColor = {};

        struct Depth
        {
            f32 Depth = 0.f;
            u8 Stencil = 0;
        } DepthStencilColor;

        bool IsDepth = false;
    };

    struct CommandRenderPassBeginInfo
    {
        u32 ClearValueCount = 0;
        ClearValue pClearValues[APIConfig::kMaxColorAttachmentCount] = {};

        // WH(-1) means we cover entire window/screen, info from swapchain
        XMUINT4 RenderArea = { 0, 0, UINT32_MAX, UINT32_MAX };
    };

    struct VKCommandAllocator
    {
        VkCommandPool pHandle = nullptr;
    };

    // A VKCommandList cannot execute itself, to execute it, we need to use VKCommandQueue
    struct VKCommandList
    {
        void Init(VkCommandBuffer pHandle, CommandListType type, VkFence pFence);

        void SetViewport(u32 id, u32 width, u32 height, f32 minDepth, f32 maxDepth);
        void SetScissor(u32 id, u32 x, u32 y, u32 w, u32 h);

        /// Buffer Commands
        void SetVertexBuffer(VKBuffer *pBuffer);
        void SetIndexBuffer(VKBuffer *pBuffer, bool type32 = true);
        void CopyBuffer(VKBuffer *pSource, VKBuffer *pDest, u32 size);

        /// Draw Commands
        void Draw(u32 vertexCount, u32 firstVertex = 0, u32 instanceCount = 1, u32 firstInstance = 1);
        void DrawIndexed(u32 indexCount, u32 firstIndex = 0, u32 vertexOffset = 0, u32 instanceCount = 1, u32 firstInstance = 1);

        /// RenderPass Commands

        // If pRenderPass is a nullptr, we use SwapChain's renderpass
        void BeginRenderPass(CommandRenderPassBeginInfo &beginInfo, VkRenderPass pRenderPass = nullptr);
        void EndRenderPass();

        // Pipeline
        void SetPipeline(VKPipeline *pPipeline);
        void SetPipelineDescriptorSets(const std::initializer_list<VKDescriptorSet *> &sets);

        CommandListType m_Type;
        VkCommandBuffer m_pHandle = nullptr;
        VKCommandAllocator m_Allocator;
        VkFence m_pFence = nullptr;

        VKPipeline *m_pSetPipeline = nullptr;
    };

    struct CommandListPool
    {
        void Init();

        VKCommandList *Request(CommandListType type);
        void SetExecuteState(VKCommandList *pList);
        void Discard(VKCommandList *pList);

        void WaitForAll();

        eastl::array<VKCommandList, 32> m_DirectLists;
        eastl::array<VKCommandList, 16> m_ComputeLists;
        eastl::array<VKCommandList, 8> m_CopyLists;

        eastl::atomic<u32> m_DirectListMask;
        eastl::atomic<u32> m_DirectFenceMask;

        eastl::atomic<u16> m_ComputeListMask;
        eastl::atomic<u8> m_CopyListMask;
    };

}  // namespace lr::Graphics