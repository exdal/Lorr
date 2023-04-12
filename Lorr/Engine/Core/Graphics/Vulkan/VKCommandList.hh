//
// Created on Tuesday 10th May 2022 by exdal
//

#pragma once

#include "Allocator.hh"

#include "VKCommandQueue.hh"
#include "VKPipeline.hh"
#include "VKResource.hh"

#include "Vulkan.hh"

namespace lr::Graphics
{
enum CommandListType : u32
{
    LR_COMMAND_LIST_TYPE_GRAPHICS = 0,
    LR_COMMAND_LIST_TYPE_COMPUTE,
    LR_COMMAND_LIST_TYPE_TRANSFER,
    LR_COMMAND_LIST_TYPE_COUNT,
};

enum CommandListUsage : u32
{
    LR_COMMAND_LIST_USAGE_ONE_TIME = 0,
    LR_COMMAND_LIST_USAGE_SIMULTANEOUS,
};

enum PipelineStage : u32
{
    LR_PIPELINE_STAGE_NONE = 0,
    LR_PIPELINE_STAGE_VERTEX_INPUT = 1 << 0,
    LR_PIPELINE_STAGE_VERTEX_SHADER = 1 << 1,
    LR_PIPELINE_STAGE_PIXEL_SHADER = 1 << 2,
    LR_PIPELINE_STAGE_EARLY_PIXEL_TESTS = 1 << 3,
    LR_PIPELINE_STAGE_LATE_PIXEL_TESTS = 1 << 4,
    LR_PIPELINE_STAGE_COLOR_ATTACHMENT = 1 << 5,
    LR_PIPELINE_STAGE_COMPUTE_SHADER = 1 << 6,
    LR_PIPELINE_STAGE_TRANSFER = 1 << 7,
    LR_PIPELINE_STAGE_ALL_COMMANDS = 1 << 8,
    LR_PIPELINE_STAGE_HOST = 1 << 9,
};
EnumFlags(PipelineStage);

// clang-format off
enum MemoryAccess : u32
{
    LR_MEMORY_ACCESS_NONE = 0,
    LR_MEMORY_ACCESS_VERTEX_ATTRIB_READ = 1 << 0,
    LR_MEMORY_ACCESS_INDEX_ATTRIB_READ = 1 << 1,
    LR_MEMORY_ACCESS_SHADER_READ = 1 << 2,
    LR_MEMORY_ACCESS_SHADER_WRITE = 1 << 3,
    LR_MEMORY_ACCESS_COLOR_ATTACHMENT_READ = 1 << 4,
    LR_MEMORY_ACCESS_COLOR_ATTACHMENT_WRITE = 1 << 5,
    LR_MEMORY_ACCESS_DEPTH_STENCIL_READ = 1 << 6,
    LR_MEMORY_ACCESS_DEPTH_STENCIL_WRITE = 1 << 7,
    LR_MEMORY_ACCESS_TRANSFER_READ = 1 << 8,
    LR_MEMORY_ACCESS_TRANSFER_WRITE = 1 << 9,
    LR_MEMORY_ACCESS_MEMORY_READ = 1 << 10,
    LR_MEMORY_ACCESS_MEMORY_WRITE = 1 << 11,
    LR_MEMORY_ACCESS_HOST_READ = 1 << 12,
    LR_MEMORY_ACCESS_HOST_WRITE = 1 << 13,
    LR_MEMORY_ACCESS_PRESENT = 1 << 30,  // virtual flag, does not have an actual use in bare api 

    // Utility flags
    LR_MEMORY_ACCESS_COLOR_ATTACHMENT_UTL = LR_MEMORY_ACCESS_COLOR_ATTACHMENT_READ | LR_MEMORY_ACCESS_COLOR_ATTACHMENT_WRITE,
    LR_MEMORY_ACCESS_SHADER_RESOURCE_UTL = LR_MEMORY_ACCESS_SHADER_READ | LR_MEMORY_ACCESS_SHADER_WRITE,
    LR_MEMORY_ACCESS_DEPTH_STENCIL_UTL = LR_MEMORY_ACCESS_DEPTH_STENCIL_READ | LR_MEMORY_ACCESS_DEPTH_STENCIL_WRITE,
    LR_MEMORY_ACCESS_TRANSFER_UTL = LR_MEMORY_ACCESS_TRANSFER_READ | LR_MEMORY_ACCESS_TRANSFER_WRITE,
    LR_MEMORY_ACCESS_HOST_UTL = LR_MEMORY_ACCESS_HOST_READ | LR_MEMORY_ACCESS_HOST_WRITE,

    LR_MEMORY_ACCESS_IMAGE_READ_UTL = LR_MEMORY_ACCESS_SHADER_READ | LR_MEMORY_ACCESS_COLOR_ATTACHMENT_READ | LR_MEMORY_ACCESS_DEPTH_STENCIL_READ | LR_MEMORY_ACCESS_MEMORY_READ,
    LR_MEMORY_ACCESS_IMAGE_WRITE_UTL = LR_MEMORY_ACCESS_SHADER_WRITE | LR_MEMORY_ACCESS_COLOR_ATTACHMENT_WRITE | LR_MEMORY_ACCESS_DEPTH_STENCIL_WRITE | LR_MEMORY_ACCESS_MEMORY_WRITE,

};
EnumFlags(MemoryAccess);

// clang-format on

enum AttachmentOp : u32
{
    LR_ATTACHMENT_OP_DONT_CARE = 0,
    LR_ATTACHMENT_OP_LOAD,
    LR_ATTACHMENT_OP_STORE,
    LR_ATTACHMENT_OP_CLEAR,
};

struct RenderingColorAttachment
{
    Image *m_pImage = nullptr;
    ImageLayout m_Layout = LR_IMAGE_LAYOUT_UNDEFINED;
    AttachmentOp m_LoadOp : 3;
    AttachmentOp m_StoreOp : 3;
    ColorClearValue m_ClearValue = {};
};

struct RenderingDepthAttachment
{
    Image *m_pImage = nullptr;
    ImageLayout m_Layout = LR_IMAGE_LAYOUT_UNDEFINED;
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
    ImageLayout m_SrcLayout = LR_IMAGE_LAYOUT_UNDEFINED;
    ImageLayout m_DstLayout = LR_IMAGE_LAYOUT_UNDEFINED;
    PipelineStage m_SrcStage = LR_PIPELINE_STAGE_NONE;
    PipelineStage m_DstStage = LR_PIPELINE_STAGE_NONE;
    MemoryAccess m_SrcAccess = LR_MEMORY_ACCESS_NONE;
    MemoryAccess m_DstAccess = LR_MEMORY_ACCESS_NONE;
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
    void SetDescriptorSets(const std::initializer_list<DescriptorSet *> &sets);
    void SetPushConstants(ShaderStage stage, u32 offset, void *pData, u32 dataSize);

    CommandListType m_Type = LR_COMMAND_LIST_TYPE_COUNT;
    VkCommandBuffer m_pHandle = VK_NULL_HANDLE;
    CommandAllocator *m_pAllocator = nullptr;

    Pipeline *m_pPipeline = nullptr;
};

struct CommandListSubmitDesc : VkCommandBufferSubmitInfo
{
    CommandListSubmitDesc(CommandList *pList);
};

}  // namespace lr::Graphics