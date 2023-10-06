// Created on Monday July 18th 2022 by exdal
// Last modified on Monday August 28th 2023 by exdal

#pragma once

#include "Memory/MemoryUtils.hh"

#include "APIAllocator.hh"
#include "APIResource.hh"
#include "Pipeline.hh"

namespace lr::Graphics
{
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
        Image *pImage,
        ImageLayout layout,
        AttachmentOp loadOp,
        AttachmentOp storeOp,
        ColorClearValue clearVal);
    RenderingAttachment(
        Image *pImage,
        ImageLayout layout,
        AttachmentOp loadOp,
        AttachmentOp storeOp,
        DepthClearValue clearVal);

private:
    void InitImage(
        Image *pImage, ImageLayout layout, AttachmentOp loadOp, AttachmentOp storeOp);
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
    u64 m_Value = 0;
    VkSemaphore m_pHandle = VK_NULL_HANDLE;
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
    void CopyBufferToBuffer(
        Buffer *pSource, Buffer *pDest, u64 srcOff, u64 dstOff, u64 size);
    void CopyBufferToImage(
        Buffer *pSource, Image *pDest, ImageUsage aspectUsage, ImageLayout layout);

    /// Draw Commands
    void Draw(
        u32 vertexCount,
        u32 firstVertex = 0,
        u32 instanceCount = 1,
        u32 firstInstance = 0);
    void DrawIndexed(
        u32 indexCount,
        u32 firstIndex = 0,
        u32 vertexOffset = 0,
        u32 instanceCount = 1,
        u32 firstInstance = 0);
    void Dispatch(u32 groupX, u32 groupY, u32 groupZ);

    /// Pipeline States
    void SetViewport(u32 id, u32 x, u32 y, u32 width, u32 height);
    void SetScissors(u32 id, u32 x, u32 y, u32 width, u32 height);
    void SetPrimitiveType(PrimitiveType type);

    void SetPipeline(Pipeline *pPipeline);
    void SetPushConstants(
        void *pData, u32 dataSize, u32 offset, ShaderStage stage = ShaderStage::All);
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