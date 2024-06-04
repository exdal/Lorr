#pragma once

#include "Common.hh"
#include "zfwd.hh"

namespace lr::graphics {
struct Semaphore {
    u64 advance() { return ++m_counter; }
    u64 &counter() { return m_counter; }

    u64 m_counter = 0;
    VkSemaphore m_handle = VK_NULL_HANDLE;

    constexpr static auto OBJECT_TYPE = VK_OBJECT_TYPE_SEMAPHORE;
    operator auto &() { return m_handle; }
    operator const auto &() const { return m_handle; }
    explicit operator bool() { return m_handle != nullptr; }
};

struct TimestampQueryPoolInfo {
    u32 query_count = 0;
    std::string_view debug_name = {};
};

struct TimestampQueryPool {
    VkQueryPool m_handle = VK_NULL_HANDLE;

    constexpr static auto OBJECT_TYPE = VK_OBJECT_TYPE_QUERY_POOL;
    operator auto &() { return m_handle; }
    operator const auto &() const { return m_handle; }
    explicit operator bool() { return m_handle != VK_NULL_HANDLE; }
};

/// COMMAND ALLOCATOR ///
struct CommandAllocatorInfo {
    CommandType type = CommandType::Count;
    CommandAllocatorFlag flags = CommandAllocatorFlag::None;
    std::string_view debug_name = {};
};

struct CommandAllocator {
    CommandType m_type = CommandType::Count;
    CommandQueue *m_queue = nullptr;
    VkCommandPool m_handle = VK_NULL_HANDLE;

    constexpr static auto OBJECT_TYPE = VK_OBJECT_TYPE_COMMAND_POOL;
    operator auto &() { return m_handle; }
    operator const auto &() const { return m_handle; }
    explicit operator bool() { return m_handle != VK_NULL_HANDLE; }
};

/// COMMAND LIST ///
struct CommandList {
    // Debugging
    void reset_query_pool(TimestampQueryPool &query_pool, u32 first_query, u32 query_count);
    void write_timestamp(TimestampQueryPool &query_pool, PipelineStage pipeline_stage, u32 query_index);

    // Barrier utils
    void image_transition(const ImageBarrier &barrier);
    void memory_barrier(const MemoryBarrier &barrier);
    void set_barriers(std::span<const MemoryBarrier> memory, std::span<const ImageBarrier> image);

    // Copy Commands
    void copy_buffer_to_buffer(BufferID src, BufferID dst, std::span<const BufferCopyRegion> regions);
    void copy_buffer_to_image(BufferID src, ImageID dst, ImageLayout layout, std::span<const ImageCopyRegion> regions);
    void blit_image(ImageID src, ImageLayout src_layout, ImageID dst, ImageLayout dst_layout, Filtering filter, std::span<const ImageBlit> blits);

    // Pipeline State
    void begin_rendering(const RenderingBeginInfo &info);
    void end_rendering();
    void set_pipeline(PipelineID pipeline_id);
    void set_push_constants(PipelineLayoutID layout_id, void *data, u32 data_size, u32 offset, ShaderStageFlag stage_flags = ShaderStageFlag::All);
    void set_descriptor_sets(PipelineLayoutID layout_id, PipelineBindPoint bind_point, u32 first_set, std::span<DescriptorSet> sets);
    void set_vertex_buffer(BufferID buffer_id, u64 offset = 0, u32 first_binding = 0, u32 binding_count = 1);
    void set_index_buffer(BufferID buffer_id, u64 offset = 0, bool use_u16 = false);

    void set_viewport(u32 id, const Viewport &viewport);
    void set_scissors(u32 id, const Rect2D &rect);

    /// Draw Commands
    void draw(u32 vertex_count, u32 first_vertex = 0, u32 instance_count = 1, u32 first_instance = 0);
    void draw_indexed(u32 index_count, u32 first_index = 0, i32 vertex_offset = 0, u32 instance_count = 1, u32 first_instance = 0);
    void dispatch(u32 group_x, u32 group_y, u32 group_z);

    CommandType m_type = CommandType::Count;
    usize m_frame_index = 0;
    Device *m_device = nullptr;

    VkCommandBuffer m_handle = VK_NULL_HANDLE;

    constexpr static auto OBJECT_TYPE = VK_OBJECT_TYPE_COMMAND_BUFFER;
    operator auto &() { return m_handle; }
    operator const auto &() const { return m_handle; }
    explicit operator bool() { return m_handle != nullptr; }
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

    CommandList &m_command_list;
};

struct QueueSubmitInfo {
    std::span<const SemaphoreSubmitInfo> additional_wait_semas = {};
    std::span<const SemaphoreSubmitInfo> additional_signal_semas = {};
};

struct CommandQueue {
    void defer(std::span<const BufferID> buffer_ids);
    void defer(std::span<const ImageID> image_ids);
    void defer(std::span<const ImageViewID> image_view_ids);
    void defer(std::span<const SamplerID> sampler_ids);
    void defer(std::span<const CommandList> command_lists);

    [[nodiscard]] CommandList &begin_command_list();
    void end_command_list(CommandList &cmd_list);

    VKResult submit(const QueueSubmitInfo &info);
    VKResult present(SwapChain &swap_chain, Semaphore &present_sema, u32 image_id);

    CommandType m_type = CommandType::Count;
    u32 m_index = 0;  // Physical device queue family index
    Semaphore m_semaphore = {};
    ls::static_vector<CommandAllocator, Limits::FrameCount> m_allocators = {};
    ls::static_vector<plf::colony<CommandList>, Limits::FrameCount> m_command_lists = {};
    ls::static_vector<std::vector<CommandListSubmitInfo>, Limits::FrameCount> m_frame_cmd_submits = {};

    plf::colony<std::pair<BufferID, u64>> m_garbage_buffers = {};
    plf::colony<std::pair<ImageID, u64>> m_garbage_images = {};
    plf::colony<std::pair<ImageViewID, u64>> m_garbage_image_views = {};
    plf::colony<std::pair<SamplerID, u64>> m_garbage_samplers = {};
    plf::colony<std::pair<CommandList, u64>> m_garbage_command_lists = {};

    Device *m_device = nullptr;
    VkQueue m_handle = VK_NULL_HANDLE;

    u32 family_index() { return m_index; }
    Semaphore &semaphore() { return m_semaphore; }
    CommandAllocator &command_allocator(usize frame_index) { return m_allocators[frame_index]; }

    constexpr static auto OBJECT_TYPE = VK_OBJECT_TYPE_QUEUE;
    operator auto &() { return m_handle; }
    operator const auto &() const { return m_handle; }
    explicit operator bool() { return m_handle != VK_NULL_HANDLE; }
};

}  // namespace lr::graphics
