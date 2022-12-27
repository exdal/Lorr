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
        Count,
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

    enum AttachmentOp
    {
        LR_ATTACHMENT_OP_LOAD,
        LR_ATTACHMENT_OP_STORE,
        LR_ATTACHMENT_OP_CLEAR,
        LR_ATTACHMENT_OP_DONT_CARE,
    };

    struct CommandListAttachment
    {
        BaseImage *pHandle = nullptr;
        ClearValue ClearVal;
        AttachmentOp LoadOp;
        AttachmentOp StoreOp;
    };

    struct CommandListBeginDesc
    {
        u32 ColorAttachmentCount = 0;
        CommandListAttachment pColorAttachments[LR_MAX_RENDER_TARGET_PER_PASS] = {};
        CommandListAttachment *pDepthAttachment = nullptr;

        // WH(-1) means we cover entire window/screen, info from swapchain
        XMUINT4 RenderArea = { 0, 0, UINT32_MAX, UINT32_MAX };
    };

    // Incomplete pipeline stage enums.
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

        // API exclusive pipeline enums
        LR_PIPELINE_STAGE_HOST = 1 << 20,
    };
    EnumFlags(PipelineStage);

    enum PipelineAccess : u32
    {
        LR_PIPELINE_ACCESS_NONE = 0,
        LR_PIPELINE_ACCESS_VERTEX_ATTRIB_READ = 1 << 0,
        LR_PIPELINE_ACCESS_INDEX_ATTRIB_READ = 1 << 1,
        LR_PIPELINE_ACCESS_SHADER_READ = 1 << 2,
        LR_PIPELINE_ACCESS_SHADER_WRITE = 1 << 3,
        LR_PIPELINE_ACCESS_RENDER_TARGET_READ = 1 << 4,
        LR_PIPELINE_ACCESS_RENDER_TARGET_WRITE = 1 << 5,
        LR_PIPELINE_ACCESS_DEPTH_STENCIL_READ = 1 << 6,
        LR_PIPELINE_ACCESS_DEPTH_STENCIL_WRITE = 1 << 7,
        LR_PIPELINE_ACCESS_TRANSFER_READ = 1 << 8,
        LR_PIPELINE_ACCESS_TRANSFER_WRITE = 1 << 9,
        LR_PIPELINE_ACCESS_MEMORY_READ = 1 << 10,
        LR_PIPELINE_ACCESS_MEMORY_WRITE = 1 << 11,

        // API exclusive pipeline enums
        LR_PIPELINE_ACCESS_HOST_READ = 1 << 20,
        LR_PIPELINE_ACCESS_HOST_WRITE = 1 << 21,
    };
    EnumFlags(PipelineAccess);

    struct PipelineBarrier
    {
        ResourceUsage CurrentUsage = LR_RESOURCE_USAGE_UNKNOWN;
        PipelineStage CurrentStage = LR_PIPELINE_STAGE_NONE;
        PipelineAccess CurrentAccess = LR_PIPELINE_ACCESS_NONE;
        ResourceUsage NextUsage = LR_RESOURCE_USAGE_UNKNOWN;
        PipelineStage NextStage = LR_PIPELINE_STAGE_NONE;
        PipelineAccess NextAccess = LR_PIPELINE_ACCESS_NONE;
    };

    struct BaseCommandList
    {
        /// Render Pass
        virtual void BeginPass(CommandListBeginDesc *pDesc) = 0;
        virtual void EndPass() = 0;
        virtual void ClearImage(BaseImage *pImage, ClearValue val) = 0;

        /// Memory Barriers
        virtual void SetImageBarrier(BaseImage *pImage, PipelineBarrier *pBarrier) = 0;
        virtual void SetBufferBarrier(BaseBuffer *pBuffer, PipelineBarrier *pBarrier) = 0;

        /// Buffer Commands
        virtual void SetVertexBuffer(BaseBuffer *pBuffer) = 0;
        virtual void SetIndexBuffer(BaseBuffer *pBuffer, bool type32 = true) = 0;
        virtual void CopyBuffer(BaseBuffer *pSource, BaseBuffer *pDest, u32 size) = 0;
        virtual void CopyBuffer(BaseBuffer *pSource, BaseImage *pDest) = 0;

        /// Draw Commands
        virtual void Draw(u32 vertexCount, u32 firstVertex = 0, u32 instanceCount = 1, u32 firstInstance = 1) = 0;
        virtual void DrawIndexed(u32 indexCount, u32 firstIndex = 0, u32 vertexOffset = 0, u32 instanceCount = 1, u32 firstInstance = 0) = 0;
        virtual void Dispatch(u32 groupX, u32 groupY, u32 groupZ) = 0;

        /// Pipeline States
        virtual void SetViewport(u32 id, u32 x, u32 y, u32 width, u32 height) = 0;
        virtual void SetScissors(u32 id, u32 x, u32 y, u32 width, u32 height) = 0;
        virtual void SetPrimitiveType(PrimitiveType type) = 0;

        virtual void SetGraphicsPipeline(BasePipeline *pPipeline) = 0;
        virtual void SetComputePipeline(BasePipeline *pPipeline) = 0;
        virtual void SetGraphicsDescriptorSets(const std::initializer_list<BaseDescriptorSet *> &sets) = 0;
        virtual void SetComputeDescriptorSets(const std::initializer_list<BaseDescriptorSet *> &sets) = 0;
        virtual void SetGraphicsPushConstants(BasePipeline *pPipeline, ShaderStage stage, void *pData, u32 dataSize) = 0;
        virtual void SetComputePushConstants(BasePipeline *pPipeline, void *pData, u32 dataSize) = 0;

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