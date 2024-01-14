#pragma once

#include "APIObject.hh"
#include "Resource.hh"
#include "Pipeline.hh"

namespace lr::Graphics
{
struct DescriptorBufferBindInfo : VkDescriptorBufferBindingInfoEXT
{
    DescriptorBufferBindInfo(Buffer *buffer, BufferUsage buffer_usage);
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
    RenderingAttachment(ImageView *image_view, ImageLayout layout, AttachmentOp load_op, AttachmentOp store_op);
    RenderingAttachment(ImageView *image_view, ImageLayout layout, AttachmentOp load_op, AttachmentOp store_op, ColorClearValue clear_val);
    RenderingAttachment(ImageView *image_view, ImageLayout layout, AttachmentOp load_op, AttachmentOp store_op, DepthClearValue clear_val);
};

constexpr static glm::uvec4 render_area_whole = { 0, 0, -1, -1 };
struct RenderingBeginDesc
{
    glm::uvec4 m_render_area = render_area_whole;

    eastl::span<RenderingAttachment> m_color_attachments;
    RenderingAttachment *m_depth_attachment = nullptr;
};

struct CommandQueue;
struct PipelineBarrier
{
    ImageLayout m_src_layout = ImageLayout::Undefined;
    ImageLayout m_dst_layout = ImageLayout::Undefined;
    PipelineStage m_src_stage = PipelineStage::None;
    PipelineStage m_dst_stage = PipelineStage::None;
    MemoryAccess m_src_access = MemoryAccess::None;
    MemoryAccess m_dst_access = MemoryAccess::None;
    u32 m_src_queue_index = ~0;
    u32 m_dst_queue_index = ~0;
};

// Utility classes to help us batch the barriers

struct ImageBarrier : VkImageMemoryBarrier2
{
    ImageBarrier() = default;
    ImageBarrier(Image *image, ImageSubresourceInfo slice_info, const PipelineBarrier &barrier);
};

// fuck yourself holy shit
#ifdef MemoryBarrier
#undef MemoryBarrier
#endif

struct MemoryBarrier : VkMemoryBarrier2
{
    MemoryBarrier() = default;
    MemoryBarrier(const PipelineBarrier &barrier);
};

struct DependencyInfo : VkDependencyInfo
{
    DependencyInfo();
    DependencyInfo(eastl::span<ImageBarrier> image_barriers, eastl::span<MemoryBarrier> memory_barriers);
};

struct Semaphore : Tracked<VkSemaphore>
{
    u64 m_value = 0;
};
LR_ASSIGN_OBJECT_TYPE(Semaphore, VK_OBJECT_TYPE_SEMAPHORE);

struct SemaphoreSubmitDesc : VkSemaphoreSubmitInfo
{
    SemaphoreSubmitDesc(Semaphore *semaphore, PipelineStage stage);
    SemaphoreSubmitDesc(Semaphore *semaphore, u64 value, PipelineStage stage);
    SemaphoreSubmitDesc(Semaphore *semaphore, u64 value);
    SemaphoreSubmitDesc() = default;
};

struct CommandQueue : Tracked<VkQueue>
{
};
LR_ASSIGN_OBJECT_TYPE(CommandQueue, VK_OBJECT_TYPE_QUEUE);

struct CommandAllocator : Tracked<VkCommandPool>
{
};
LR_ASSIGN_OBJECT_TYPE(CommandAllocator, VK_OBJECT_TYPE_COMMAND_POOL);

struct BufferCopyRegion : VkBufferCopy
{
    BufferCopyRegion(u64 src_offset, u64 dst_offset, u64 size);
};

struct ImageCopyRegion : VkBufferImageCopy
{
    ImageCopyRegion(const VkBufferImageCopy &lazy);
    ImageCopyRegion(ImageSubresourceInfo slice_info, u32 width, u32 height, u64 buffer_offset);
};

struct CommandList : Tracked<VkCommandBuffer>
{
    void begin_rendering(const RenderingBeginDesc &desc);
    void end_rendering();

    /// Memory Barriers
    void set_pipeline_barrier(DependencyInfo *dependency_info);

    /// Copy Commands
    void copy_buffer_to_buffer(Buffer *src, Buffer *dst, eastl::span<BufferCopyRegion> regions);
    void copy_buffer_to_image(Buffer *src, Image *dst, ImageLayout layout, eastl::span<ImageCopyRegion> regions);

    /// Draw Commands
    void draw(u32 vertex_count, u32 first_vertex = 0, u32 instance_count = 1, u32 first_instance = 0);
    void draw_indexed(u32 index_count, u32 first_index = 0, u32 vertex_offset = 0, u32 instance_count = 1, u32 first_instance = 0);
    void dispatch(u32 group_x, u32 group_y, u32 group_z);

    /// Pipeline States
    void set_viewport(u32 id, const glm::uvec4 &xywh, const glm::vec2 &depth = { 0.01, 1.0 });
    void set_scissors(u32 id, const glm::ivec4 &xywh);
    void set_primitive_type(PrimitiveType type);

    void set_pipeline(Pipeline *pipeline);
    void set_push_constants(void *data, u32 data_size, u32 offset, PipelineLayout *layout, ShaderStage stage_flags = ShaderStage::All);
    void set_descriptor_sets(PipelineBindPoint bind_point, PipelineLayout *layout, u32 first_set, eastl::span<DescriptorSet> sets);
    void set_descriptor_buffers(eastl::span<DescriptorBufferBindInfo> binding_infos);
    void set_descriptor_buffer_offsets(
        PipelineLayout *layout, PipelineBindPoint bind_point, u32 first_set, eastl::span<u32> indices, eastl::span<u64> offsets);
};
LR_ASSIGN_OBJECT_TYPE(CommandList, VK_OBJECT_TYPE_COMMAND_BUFFER);

struct CommandListSubmitDesc : VkCommandBufferSubmitInfo
{
    CommandListSubmitDesc() = default;
    CommandListSubmitDesc(CommandList *list);
};

}  // namespace lr::Graphics
