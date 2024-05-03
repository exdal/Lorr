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

struct CommandQueue {
    u32 family_index() { return m_index; }
    Semaphore &semaphore() { return m_semaphore; }

    VKResult submit(QueueSubmitInfo &submit_info);
    VKResult present(SwapChain &swap_chain, Semaphore &present_sema, u32 image_id);

    u32 m_index = 0;  // Physical device queue family index
    Semaphore m_semaphore = {};
    VkQueue m_handle = VK_NULL_HANDLE;

    constexpr static auto OBJECT_TYPE = VK_OBJECT_TYPE_QUEUE;
    operator auto &() { return m_handle; }
    operator const auto &() const { return m_handle; }
    explicit operator bool() { return m_handle != VK_NULL_HANDLE; }
};

/// COMMAND ALLOCATOR ///
struct CommandAllocatorInfo {
    CommandType type = CommandType::Count;
    CommandAllocatorFlag flags = CommandAllocatorFlag::None;
};

struct CommandAllocator {
    VkCommandPool m_handle = VK_NULL_HANDLE;

    constexpr static auto OBJECT_TYPE = VK_OBJECT_TYPE_COMMAND_POOL;
    operator auto &() { return m_handle; }
    operator const auto &() const { return m_handle; }
    explicit operator bool() { return m_handle != VK_NULL_HANDLE; }
};

/// COMMAND LIST ///
struct CommandList {
    // Memory Barriers
    void set_pipeline_barrier(DependencyInfo &dependency_info);

    // Copy Commands
    void copy_buffer_to_buffer(Buffer *src, Buffer *dst, std::span<BufferCopyRegion> regions);
    void copy_buffer_to_image(Buffer *src, Image *dst, ImageLayout layout, std::span<ImageCopyRegion> regions);

    // Pipeline State
    void begin_rendering(const RenderingBeginInfo &info);
    void end_rendering();
    void set_pipeline(Pipeline *pipeline);
    void set_push_constants(
        PipelineLayout *layout, void *data, u32 data_size, u32 offset, ShaderStageFlag stage_flags = ShaderStageFlag::All);
    void set_descriptor_sets(
        PipelineLayout *layout, PipelineBindPoint bind_point, u32 first_set, const ls::static_vector<VkDescriptorSet, 16> &sets);
    void set_vertex_buffer(Buffer *buffer, u64 offset = 0, u32 first_binding = 0, u32 binding_count = 1);
    void set_index_buffer(Buffer *buffer, u64 offset = 0, bool use_u16 = false);

    void set_viewport(u32 id, const Viewport &viewport);
    void set_scissors(u32 id, const Rect2D &rect);

    /// Draw Commands
    void draw(u32 vertex_count, u32 first_vertex = 0, u32 instance_count = 1, u32 first_instance = 0);
    void draw_indexed(u32 index_count, u32 first_index = 0, i32 vertex_offset = 0, u32 instance_count = 1, u32 first_instance = 0);
    void dispatch(u32 group_x, u32 group_y, u32 group_z);

    VkCommandBuffer m_handle = VK_NULL_HANDLE;
    CommandAllocator *m_allocator = nullptr;

    constexpr static auto OBJECT_TYPE = VK_OBJECT_TYPE_COMMAND_BUFFER;
    operator auto &() { return m_handle; }
    operator const auto &() const { return m_handle; }
    explicit operator bool() { return m_handle != nullptr; }
};

struct CommandBatcher {
    constexpr static usize kMaxMemoryBarriers = 16;
    constexpr static usize kMaxImageBarriers = 16;

    CommandBatcher(Device *device, CommandList &command_list);
    CommandBatcher(Unique<CommandList> &command_list);
    ~CommandBatcher();

    void insert_memory_barrier(const MemoryBarrier &barrier);
    void insert_image_barrier(const ImageBarrier &barrier);
    void flush_barriers();

    ls::static_vector<MemoryBarrier, kMaxMemoryBarriers> m_memory_barriers = {};
    ls::static_vector<ImageBarrier, kMaxImageBarriers> m_image_barriers = {};

    Device *m_device;
    CommandList &m_command_list;
};
}  // namespace lr::graphics
