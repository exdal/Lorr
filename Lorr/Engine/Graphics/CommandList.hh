//
// Created on Tuesday 10th May 2022 by exdal
//

#pragma once

#include "APIAllocator.hh"

#include "CommandQueue.hh"
#include "Pipeline.hh"

namespace lr::Graphics
{
enum class CommandType : u32
{
    Graphics = 0,
    Compute,
    Transfer,
    Count,
};

enum class PipelineStage : u32
{
    None = 0,

    /// ---- IN ORDER ----
    DrawIndirect = 1 << 0,

    /// GRAPHICS PIPELINE
    VertexAttribInput = 1 << 1,
    IndexAttribInput = 1 << 2,
    VertexShader = 1 << 3,
    TessellationControl = 1 << 4,
    TessellationEvaluation = 1 << 5,
    PixelShader = 1 << 6,
    EarlyPixelTests = 1 << 7,
    LatePixelTests = 1 << 8,
    ColorAttachmentOutput = 1 << 9,
    AllGraphics = 1 << 10,

    /// COMPUTE PIPELINE
    ComputeShader = 1 << 11,

    /// TRANSFER PIPELINE
    Host = 1 << 12,  // not really in transfer but eh
    Copy = 1 << 13,
    Bilt = 1 << 14,
    Resolve = 1 << 15,
    Clear = 1 << 16,
    AllTransfer = 1 << 17,

    /// OTHER STAGES
    AllCommands = 1 << 18,
};
EnumFlags(PipelineStage);

enum class MemoryAccess : u32
{
    None = 0,
    IndirectRead = 1 << 0,
    VertexAttribRead = 1 << 1,
    IndexAttribRead = 1 << 2,
    InputAttachmentRead = 1 << 3,
    UniformRead = 1 << 4,
    SampledRead = 1 << 5,
    StorageRead = 1 << 6,
    StorageWrite = 1 << 7,
    ColorAttachmentRead = 1 << 8,
    ColorAttachmentWrite = 1 << 9,
    DepthStencilRead = 1 << 10,
    DepthStencilWrite = 1 << 11,
    TransferRead = 1 << 12,
    TransferWrite = 1 << 13,
    HostRead = 1 << 14,
    HostWrite = 1 << 15,
    MemoryRead = 1 << 16,  // MEMORY_# flags are more general version of specific flags above
    MemoryWrite = 1 << 17,

    /// VIRTUAL FLAGS
    Present = 1 << 30,

    /// UTILITY FLAGS
    ColorAttachmentAll = ColorAttachmentRead | ColorAttachmentWrite,
    DepthStencilAttachmentAll = DepthStencilRead | DepthStencilWrite,
    TransferAll = TransferRead | TransferWrite,
    HostAll = HostRead | HostWrite,

    ImageReadUTL = SampledRead | StorageRead | ColorAttachmentRead | DepthStencilRead | MemoryRead,
    ImageWriteUTL = StorageWrite | ColorAttachmentWrite | DepthStencilWrite | MemoryWrite,

};
EnumFlags(MemoryAccess);

enum class AttachmentOp : u32
{
    DontCare = 0,
    Load,
    Store,
    Clear,
};

struct RenderingColorAttachment
{
    Image *m_pImage = nullptr;
    ImageLayout m_Layout = ImageLayout::Undefined;
    AttachmentOp m_LoadOp : 3;
    AttachmentOp m_StoreOp : 3;
    ColorClearValue m_ClearValue = {};
};

struct RenderingDepthAttachment
{
    Image *m_pImage = nullptr;
    ImageLayout m_Layout = ImageLayout::Undefined;
    AttachmentOp m_LoadOp : 3;
    AttachmentOp m_StoreOp : 3;
    DepthClearValue m_ClearValue = {};
};

struct RenderingBeginDesc
{
    XMUINT4 m_RenderArea = { 0, 0, UINT32_MAX, UINT32_MAX };

    eastl::span<RenderingColorAttachment> m_ColorAttachments;
    RenderingDepthAttachment *m_pDepthAttachment = nullptr;
};

struct PipelineBarrier
{
    ImageLayout m_SrcLayout = ImageLayout::Undefined;
    ImageLayout m_DstLayout = ImageLayout::Undefined;
    PipelineStage m_SrcStage = PipelineStage::None;
    PipelineStage m_DstStage = PipelineStage::None;
    MemoryAccess m_SrcAccess = MemoryAccess::None;
    MemoryAccess m_DstAccess = MemoryAccess::None;
    u32 m_SrcQueue = ~0;
    u32 m_DstQueue = ~0;
};

// Utility classes to help us batch the barriers

#ifdef MemoryBarrier  // fuck you
#undef MemoryBarrier
#endif

struct ImageBarrier : VkImageMemoryBarrier2
{
    ImageBarrier() = default;
    ImageBarrier(Image *pImage, const PipelineBarrier &barrier);
};

struct BufferBarrier : VkBufferMemoryBarrier2
{
    BufferBarrier() = default;
    BufferBarrier(Buffer *pBuffer, const PipelineBarrier &barrier);
};

struct MemoryBarrier : VkMemoryBarrier2
{
    MemoryBarrier() = default;
    MemoryBarrier(const PipelineBarrier &barrier);
};

struct DependencyInfo : VkDependencyInfo
{
    DependencyInfo();
    DependencyInfo(eastl::span<ImageBarrier> imageBarriers);
    DependencyInfo(eastl::span<BufferBarrier> bufferBarriers);
    DependencyInfo(eastl::span<MemoryBarrier> memoryBarriers);
    void SetImageBarriers(eastl::span<ImageBarrier> imageBarriers);
    void SetBufferBarriers(eastl::span<BufferBarrier> bufferBarriers);
    void SetMemoryBarriers(eastl::span<MemoryBarrier> memoryBarriers);
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
    void BeginRendering(RenderingBeginDesc *pDesc);
    void EndRendering();

    /// Memory Barriers
    void SetPipelineBarrier(DependencyInfo *pDependencyInfo);

    /// Buffer Commands
    void SetVertexBuffer(Buffer *pBuffer);
    void SetIndexBuffer(Buffer *pBuffer, bool type32 = true);
    void CopyBuffer(Buffer *pSource, Buffer *pDest, u32 size);
    void CopyBuffer(Buffer *pSource, Image *pDest, ImageLayout layout);

    /// Draw Commands
    void Draw(u32 vertexCount, u32 firstVertex = 0, u32 instanceCount = 1, u32 firstInstance = 1);
    void DrawIndexed(
        u32 indexCount, u32 firstIndex = 0, u32 vertexOffset = 0, u32 instanceCount = 1, u32 firstInstance = 0);
    void Dispatch(u32 groupX, u32 groupY, u32 groupZ);

    /// Pipeline States
    void SetViewport(u32 id, u32 x, u32 y, u32 width, u32 height);
    void SetScissors(u32 id, u32 x, u32 y, u32 width, u32 height);
    void SetPrimitiveType(PrimitiveType type);

    void SetPipeline(Pipeline *pPipeline);
    void SetPushConstants(ShaderStage stage, u32 offset, void *pData, u32 dataSize);
    void SetDescriptorBuffers(eastl::span<DescriptorBindingInfo> bindingInfos);

    CommandType m_Type = CommandType::Count;
    VkCommandBuffer m_pHandle = VK_NULL_HANDLE;
    CommandAllocator *m_pAllocator = nullptr;

    Pipeline *m_pPipeline = nullptr;
};

struct CommandListSubmitDesc : VkCommandBufferSubmitInfo
{
    CommandListSubmitDesc(CommandList *pList);
};

}  // namespace lr::Graphics