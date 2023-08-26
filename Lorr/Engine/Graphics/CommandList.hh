// Created on Monday July 18th 2022 by exdal
// Last modified on Saturday August 26th 2023 by exdal

#pragma once

#include "APIAllocator.hh"

#include "Buffer.hh"
#include "Pipeline.hh"

namespace lr::Graphics
{
struct BindlessLayout
{
    using DescriptorIDType = u32;
    static constexpr u32 kDescriptorIDSize = sizeof(DescriptorIDType);
    static constexpr u32 kDescriptorIDCount = 8;
    static constexpr u32 kConstantDataOffset = kDescriptorIDCount * kDescriptorIDSize;

    struct Binding
    {
        Binding(u32 index, Buffer *pBuffer)
            : m_Index(index),
              m_DescriptorIndex(pBuffer->m_DescriptorIndex){};
        Binding(u32 index, Image *pImage)
            : m_Index(index),
              m_DescriptorIndex(pImage->m_DescriptorIndex){};
        Binding(u32 index, Sampler *pSampler)
            : m_Index(index),
              m_DescriptorIndex(pSampler->m_DescriptorIndex){};

        u32 m_Index = 0;
        u32 m_DescriptorIndex = 0;
    };

    BindlessLayout(eastl::span<Binding> bindings)
    {
        memset(&m_Data[0], ~0, m_Data.size() * kDescriptorIDSize);  // Debugging purposes

        u32 lastBinding = ~0;
        for (const Binding &binding : bindings)
        {
            m_Count++;
            lastBinding = eastl::min(lastBinding, binding.m_Index);
            m_Data[binding.m_Index] = binding.m_DescriptorIndex;
        }

        m_Offset = Memory::AlignUp(lastBinding * kDescriptorIDSize, 4);
    }

    u32 size() { return m_Count * kDescriptorIDSize; };
    const u32 size() const { return m_Count * kDescriptorIDSize; };
    const DescriptorIDType *data() const { return m_Data.data(); };

    u16 m_Offset = 0;
    u16 m_Count = 0;
    eastl::array<DescriptorIDType, kDescriptorIDCount> m_Data = {};
};

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
    BottomOfPipe = 1 << 19,
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

struct DescriptorBufferBindInfo : VkDescriptorBufferBindingInfoEXT
{
    DescriptorBufferBindInfo(Buffer *pBuffer, BufferUsage bufferUsage);
};

enum class AttachmentOp : u32
{
    DontCare = 0,
    Load,
    Store,
    Clear,
};

struct RenderingAttachment : VkRenderingAttachmentInfo
{
    RenderingAttachment(
        Image *pImage, ImageLayout layout, AttachmentOp loadOp, AttachmentOp storeOp, ColorClearValue clearVal);
    RenderingAttachment(
        Image *pImage, ImageLayout layout, AttachmentOp loadOp, AttachmentOp storeOp, DepthClearValue clearVal);

private:
    void InitImage(Image *pImage, ImageLayout layout, AttachmentOp loadOp, AttachmentOp storeOp);
};

struct RenderingBeginDesc
{
    XMUINT4 m_RenderArea = { 0, 0, UINT32_MAX, UINT32_MAX };

    eastl::span<RenderingAttachment> m_ColorAttachments;
    RenderingAttachment *m_pDepthAttachment = nullptr;
};

struct PipelineBarrier
{
    ImageLayout m_SrcLayout = ImageLayout::Undefined;
    ImageLayout m_DstLayout = ImageLayout::Undefined;
    PipelineStage m_SrcStage = PipelineStage::None;
    PipelineStage m_DstStage = PipelineStage::None;
    MemoryAccess m_SrcAccess = MemoryAccess::None;
    MemoryAccess m_DstAccess = MemoryAccess::None;
    u32 m_SrcQueue = -1u;
    u32 m_DstQueue = -1u;
};

// Utility classes to help us batch the barriers

#ifdef MemoryBarrier  // fuck you
#undef MemoryBarrier
#endif

struct ImageBarrier : VkImageMemoryBarrier2
{
    ImageBarrier() = default;
    ImageBarrier(Image *pImage, ImageUsage aspectUsage, const PipelineBarrier &barrier);
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
    void CopyBufferToBuffer(Buffer *pSource, Buffer *pDest, u64 srcOff, u64 dstOff, u64 size);
    void CopyBufferToImage(Buffer *pSource, Image *pDest, ImageUsage aspectUsage, ImageLayout layout);

    /// Draw Commands
    void Draw(u32 vertexCount, u32 firstVertex = 0, u32 instanceCount = 1, u32 firstInstance = 0);
    void DrawIndexed(
        u32 indexCount, u32 firstIndex = 0, u32 vertexOffset = 0, u32 instanceCount = 1, u32 firstInstance = 0);
    void Dispatch(u32 groupX, u32 groupY, u32 groupZ);

    /// Pipeline States
    void SetViewport(u32 id, u32 x, u32 y, u32 width, u32 height);
    void SetScissors(u32 id, u32 x, u32 y, u32 width, u32 height);
    void SetPrimitiveType(PrimitiveType type);

    void SetPipeline(Pipeline *pPipeline);
    void SetPushConstants(
        void *pData,
        u32 dataSize,
        u32 offset = BindlessLayout::kConstantDataOffset,
        ShaderStage stage = ShaderStage::All);
    void SetBindlessLayout(BindlessLayout &layout);
    void SetDescriptorBuffers(eastl::span<DescriptorBufferBindInfo> bindingInfos);
    void SetDescriptorBufferOffsets(
        u32 firstSet, u32 setCount, eastl::span<u32> indices, eastl::span<u64> offsets);

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