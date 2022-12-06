//
// Created on Sunday 23rd October 2022 by e-erdal
//

#pragma once

#include "Core/Graphics/RHI/APIConfig.hh"
#include "Core/Graphics/RHI/Common.hh"

#include "BaseResource.hh"
#include "BasePipeline.hh"

namespace lr::Graphics
{
    enum class CommandListType : u8
    {
        Direct,
        Compute,
        Copy,
        // TODO: Video types
    };

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
            f32 Depth;
            u8 Stencil;
        } DepthStencilColor;

        bool IsDepth = false;
    };

    enum class AttachmentOperation
    {
        Load,
        Store,
        Clear,
        DontCare,
    };

    struct CommandListAttachment
    {
        BaseImage *pHandle = nullptr;
        ClearValue ClearVal;
        AttachmentOperation LoadOp;
        AttachmentOperation StoreOp;
    };

    struct CommandListBeginDesc
    {
        u32 ColorAttachmentCount = 0;
        CommandListAttachment pColorAttachments[APIConfig::kMaxColorAttachmentCount] = {};
        CommandListAttachment *pDepthAttachment = nullptr;

        // WH(-1) means we cover entire window/screen, info from swapchain
        XMUINT4 RenderArea = { 0, 0, UINT32_MAX, UINT32_MAX };
    };

    struct BaseCommandList
    {
        virtual void BeginPass(CommandListBeginDesc *pDesc) = 0;
        virtual void EndPass() = 0;

        virtual void BarrierTransition(BaseImage *pImage,
                                       ResourceUsage barrierBefore,
                                       ShaderStage shaderBefore,
                                       ResourceUsage barrierAfter,
                                       ShaderStage shaderAfter) = 0;

        virtual void BarrierTransition(BaseBuffer *pBuffer,
                                       ResourceUsage barrierBefore,
                                       ShaderStage shaderBefore,
                                       ResourceUsage barrierAfter,
                                       ShaderStage shaderAfter) = 0;

        virtual void ClearImage(BaseImage *pImage, ClearValue val) = 0;

        virtual void SetViewport(u32 id, u32 x, u32 y, u32 width, u32 height) = 0;
        virtual void SetScissors(u32 id, u32 x, u32 y, u32 width, u32 height) = 0;
        virtual void SetPrimitiveType(PrimitiveType type) = 0;

        /// Buffer Commands
        virtual void SetRootConstants() = 0;
        virtual void SetVertexBuffer(BaseBuffer *pBuffer) = 0;
        virtual void SetIndexBuffer(BaseBuffer *pBuffer, bool type32 = true) = 0;
        virtual void CopyBuffer(BaseBuffer *pSource, BaseBuffer *pDest, u32 size) = 0;
        virtual void CopyBuffer(BaseBuffer *pSource, BaseImage *pDest) = 0;

        /// Draw Commands
        virtual void Draw(u32 vertexCount, u32 firstVertex = 0, u32 instanceCount = 1, u32 firstInstance = 1) = 0;
        virtual void DrawIndexed(u32 indexCount, u32 firstIndex = 0, u32 vertexOffset = 0, u32 instanceCount = 1, u32 firstInstance = 1) = 0;

        // Pipeline
        virtual void SetPipeline(BasePipeline *pPipeline) = 0;
        virtual void SetPipelineDescriptorSets(const std::initializer_list<BaseDescriptorSet *> &sets) = 0;

        CommandListType m_Type = CommandListType::Direct;
    };

    struct BaseCommandAllocator
    {
    };

    struct CommandListPool
    {
        void Init();

        BaseCommandList *Acquire(CommandListType type);
        void Release(BaseCommandList *pList);
        void SignalFence(BaseCommandList *pList);
        void ReleaseFence(u32 index);

        void WaitForAll();

        eastl::array<BaseCommandList *, 32> m_DirectLists = {};
        eastl::array<BaseCommandList *, 16> m_ComputeLists = {};
        eastl::array<BaseCommandList *, 8> m_CopyLists = {};

        eastl::atomic<u32> m_DirectListMask;
        eastl::atomic<u32> m_DirectFenceMask;

        eastl::atomic<u16> m_ComputeListMask;
        eastl::atomic<u8> m_CopyListMask;
    };

}  // namespace lr::Graphics