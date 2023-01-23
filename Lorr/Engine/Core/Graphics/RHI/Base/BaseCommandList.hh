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
            m_IsDepth = false;

            m_RenderTargetColor = color;
        }

        ClearValue(f32 depth, u8 stencil)
        {
            m_IsDepth = true;

            m_DepthStencilColor.m_Depth = depth;
            m_DepthStencilColor.m_Stencil = stencil;
        }

        XMFLOAT4 m_RenderTargetColor = {};

        struct Depth
        {
            f32 m_Depth;
            u8 m_Stencil;
        } m_DepthStencilColor;

        bool m_IsDepth = false;
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
        Image *m_pHandle = nullptr;
        ImageFormat m_Format;
        ClearValue m_ClearVal;
        AttachmentOp m_LoadOp;
        AttachmentOp m_StoreOp;
    };

    struct CommandListBeginDesc
    {
        u32 m_ColorAttachmentCount = 0;
        CommandListAttachment m_pColorAttachments[LR_MAX_RENDER_TARGET_PER_PASS] = {};
        CommandListAttachment *m_pDepthAttachment = nullptr;

        // WH(-1) means we cover entire window/screen, info from swapchain
        XMUINT4 m_RenderArea = { 0, 0, UINT32_MAX, UINT32_MAX };
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
        ResourceUsage m_CurrentUsage = LR_RESOURCE_USAGE_UNKNOWN;
        PipelineStage m_CurrentStage = LR_PIPELINE_STAGE_NONE;
        PipelineAccess m_CurrentAccess = LR_PIPELINE_ACCESS_NONE;
        ResourceUsage m_NextUsage = LR_RESOURCE_USAGE_UNKNOWN;
        PipelineStage m_NextStage = LR_PIPELINE_STAGE_NONE;
        PipelineAccess m_NextAccess = LR_PIPELINE_ACCESS_NONE;
    };

    struct CommandList
    {
        /// Render Pass
        virtual void BeginPass(CommandListBeginDesc *pDesc) = 0;
        virtual void EndPass() = 0;
        virtual void ClearImage(Image *pImage, ClearValue val) = 0;

        /// Memory Barriers
        virtual void SetImageBarrier(Image *pImage, PipelineBarrier *pBarrier) = 0;
        virtual void SetBufferBarrier(Buffer *pBuffer, PipelineBarrier *pBarrier) = 0;

        /// Buffer Commands
        virtual void SetVertexBuffer(Buffer *pBuffer) = 0;
        virtual void SetIndexBuffer(Buffer *pBuffer, bool type32 = true) = 0;
        virtual void CopyBuffer(Buffer *pSource, Buffer *pDest, u32 size) = 0;
        virtual void CopyBuffer(Buffer *pSource, Image *pDest) = 0;

        /// Draw Commands
        virtual void Draw(u32 vertexCount, u32 firstVertex = 0, u32 instanceCount = 1, u32 firstInstance = 1) = 0;
        virtual void DrawIndexed(u32 indexCount, u32 firstIndex = 0, u32 vertexOffset = 0, u32 instanceCount = 1, u32 firstInstance = 0) = 0;
        virtual void Dispatch(u32 groupX, u32 groupY, u32 groupZ) = 0;

        /// Pipeline States
        virtual void SetViewport(u32 id, u32 x, u32 y, u32 width, u32 height) = 0;
        virtual void SetScissors(u32 id, u32 x, u32 y, u32 width, u32 height) = 0;
        virtual void SetPrimitiveType(PrimitiveType type) = 0;

        virtual void SetGraphicsPipeline(Pipeline *pPipeline) = 0;
        virtual void SetComputePipeline(Pipeline *pPipeline) = 0;
        virtual void SetGraphicsDescriptorSets(const std::initializer_list<DescriptorSet *> &sets) = 0;
        virtual void SetComputeDescriptorSets(const std::initializer_list<DescriptorSet *> &sets) = 0;
        virtual void SetGraphicsPushConstants(Pipeline *pPipeline, ShaderStage stage, void *pData, u32 dataSize) = 0;
        virtual void SetComputePushConstants(Pipeline *pPipeline, void *pData, u32 dataSize) = 0;

        CommandListType m_Type = CommandListType::Direct;
    };

    struct CommandAllocator
    {
    };

    struct CommandListPool
    {
        void Init();

        CommandList *Acquire(CommandListType type);
        void Release(CommandList *pList);
        void SignalFence(CommandList *pList);
        void ReleaseFence(u32 index);

        void WaitForAll();

        eastl::array<CommandList *, 32> m_DirectLists = {};
        eastl::array<CommandList *, 16> m_ComputeLists = {};
        eastl::array<CommandList *, 8> m_CopyLists = {};

        eastl::atomic<u32> m_DirectListMask;
        eastl::atomic<u32> m_DirectFenceMask;

        eastl::atomic<u16> m_ComputeListMask;
        eastl::atomic<u8> m_CopyListMask;
    };

}  // namespace lr::Graphics