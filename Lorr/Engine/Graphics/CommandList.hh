#pragma once

#include "Common.hh"
#include "zfwd.hh"

namespace lr {
struct Semaphore {
    u64 advance(this auto &self) { return ++self.counter; }

    u64 counter = 0;
    VkSemaphore handle = VK_NULL_HANDLE;

    constexpr static auto OBJECT_TYPE = VK_OBJECT_TYPE_SEMAPHORE;
    operator auto &() { return handle; }
    operator const auto &() const { return handle; }
    explicit operator bool() { return handle != nullptr; }
};

struct TimestampQueryPoolInfo {
    u32 query_count = 0;
    std::string_view debug_name = {};
};

struct TimestampQueryPool {
    VkQueryPool handle = VK_NULL_HANDLE;

    constexpr static auto OBJECT_TYPE = VK_OBJECT_TYPE_QUERY_POOL;
    operator auto &() { return handle; }
    operator const auto &() const { return handle; }
    explicit operator bool() { return handle != VK_NULL_HANDLE; }
};

/// COMMAND ALLOCATOR ///
struct CommandAllocatorInfo {
    CommandType type = CommandType::Count;
    CommandAllocatorFlag flags = CommandAllocatorFlag::None;
    std::string_view debug_name = {};
};

struct CommandAllocator {
    CommandType type = CommandType::Count;
    VkCommandPool handle = VK_NULL_HANDLE;

    constexpr static auto OBJECT_TYPE = VK_OBJECT_TYPE_COMMAND_POOL;
    operator auto &() { return handle; }
    operator const auto &() const { return handle; }
    explicit operator bool() { return handle != VK_NULL_HANDLE; }
};

/// COMMAND LIST ///
struct CommandList {
    void begin_recording(this CommandList &);
    void end_recording(this CommandList &);

    // Debugging
    void reset_query_pool(this CommandList &, TimestampQueryPool &query_pool, u32 first_query, u32 query_count);
    void write_timestamp(this CommandList &, TimestampQueryPool &query_pool, PipelineStage pipeline_stage, u32 query_index);

    // Barrier utils
    void image_transition(this CommandList &, const ImageBarrier &barrier);
    void memory_barrier(this CommandList &, const MemoryBarrier &barrier);
    void set_barriers(this CommandList &, ls::span<MemoryBarrier> memory, ls::span<ImageBarrier> image);

    // Copy Commands
    void copy_buffer_to_buffer(this CommandList &, BufferID src, BufferID dst, ls::span<BufferCopyRegion> regions);
    void copy_buffer_to_image(this CommandList &, BufferID src, ImageID dst, ImageLayout layout, ls::span<ImageCopyRegion> regions);
    void blit_image(this CommandList &, ImageID src, ImageLayout src_layout, ImageID dst, ImageLayout dst_layout, Filtering filter, ls::span<ImageBlit> blits);

    // Pipeline State
    void begin_rendering(this CommandList &, const RenderingBeginInfo &info);
    void end_rendering(this CommandList &);
    void set_pipeline(this CommandList &, PipelineID pipeline_id);
    void set_push_constants(this CommandList &, PipelineLayoutID layout_id, void *data, u32 data_size, u32 offset, ShaderStageFlag stage_flags = ShaderStageFlag::All);
    void set_descriptor_sets(this CommandList &, PipelineLayoutID layout_id, PipelineBindPoint bind_point, u32 first_set, ls::span<DescriptorSet> sets);
    void set_vertex_buffer(this CommandList &, BufferID buffer_id, u64 offset = 0, u32 first_binding = 0, u32 binding_count = 1);
    void set_index_buffer(this CommandList &, BufferID buffer_id, u64 offset = 0, bool use_u16 = false);

    void set_viewport(this CommandList &, u32 id, const Viewport &viewport);
    void set_scissors(this CommandList &, u32 id, const Rect2D &rect);

    /// Draw Commands
    void draw(this CommandList &, u32 vertex_count, u32 first_vertex = 0, u32 instance_count = 1, u32 first_instance = 0);
    void draw_indexed(this CommandList &, u32 index_count, u32 first_index = 0, i32 vertex_offset = 0, u32 instance_count = 1, u32 first_instance = 0);
    void dispatch(this CommandList &, u32 group_x, u32 group_y, u32 group_z);

    CommandType type = CommandType::Count;
    usize rendering_frame = 0;
    CommandAllocator *bound_allocator = nullptr;
    Device *device = nullptr;

    VkCommandBuffer handle = VK_NULL_HANDLE;

    constexpr static auto OBJECT_TYPE = VK_OBJECT_TYPE_COMMAND_BUFFER;
    operator auto &() { return handle; }
    operator const auto &() const { return handle; }
    explicit operator bool() { return handle != nullptr; }
};

struct CommandBatcher {
    constexpr static usize kMaxMemoryBarriers = 16;
    constexpr static usize kMaxImageBarriers = 16;

    CommandBatcher(CommandList &command_list);
    ~CommandBatcher();

    void insert_memory_barrier(const MemoryBarrier &barrier);
    void insert_image_barrier(const ImageBarrier &barrier);
    void flush_barriers();

    ls::static_vector<MemoryBarrier, kMaxMemoryBarriers> m_memory_barriers = {};
    ls::static_vector<ImageBarrier, kMaxImageBarriers> m_image_barriers = {};

    CommandList &command_list;
};

struct QueueSubmitInfo {
    bool self_wait = true;
    ls::span<SemaphoreSubmitInfo> additional_wait_semas = {};
    ls::span<SemaphoreSubmitInfo> additional_signal_semas = {};
    ls::span<CommandListSubmitInfo> additional_command_lists = {};
};

struct CommandQueue {
    CommandType type = CommandType::Count;
    u32 family_index = 0;  // Physical device queue family index
    Semaphore semaphore = {};
    ls::static_vector<CommandAllocator, Limits::FrameCount> allocators = {};
    ls::static_vector<plf::colony<CommandList>, Limits::FrameCount> command_lists = {};
    ls::static_vector<std::vector<CommandListSubmitInfo>, Limits::FrameCount> frame_cmd_submits = {};

    plf::colony<std::pair<BufferID, u64>> garbage_buffers = {};
    plf::colony<std::pair<ImageID, u64>> garbage_images = {};
    plf::colony<std::pair<ImageViewID, u64>> garbage_image_views = {};
    plf::colony<std::pair<SamplerID, u64>> garbage_samplers = {};
    plf::colony<std::pair<CommandList, u64>> garbage_command_lists = {};

    Device *device = nullptr;
    VkQueue handle = VK_NULL_HANDLE;

    void defer(this CommandQueue &, ls::span<BufferID> buffer_ids);
    void defer(this CommandQueue &, ls::span<ImageID> image_ids);
    void defer(this CommandQueue &, ls::span<ImageViewID> image_view_ids);
    void defer(this CommandQueue &, ls::span<SamplerID> sampler_ids);
    void defer(this CommandQueue &, ls::span<CommandList> command_lists);

    [[nodiscard]] CommandList &begin_command_list(this CommandQueue &, usize frame_index, std::source_location LOC = std::source_location::current());
    void end_command_list(this CommandQueue &, CommandList &cmd_list);

    VKResult submit(this CommandQueue &, usize frame_index, const QueueSubmitInfo &info);
    VKResult present(this CommandQueue &, SwapChain &swap_chain, Semaphore &present_sema, u32 image_id);
    void wait_for_work(this CommandQueue &);

    CommandAllocator &command_allocator(this auto &self, usize frame_index) { return self.allocators[frame_index]; }

    constexpr static auto OBJECT_TYPE = VK_OBJECT_TYPE_QUEUE;
    operator auto &() { return handle; }
    operator const auto &() const { return handle; }
    explicit operator bool() { return handle != VK_NULL_HANDLE; }
};

}  // namespace lr
