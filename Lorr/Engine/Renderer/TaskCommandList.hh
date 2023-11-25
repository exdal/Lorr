#pragma once

#include "Graphics/CommandList.hh"

namespace lr::Renderer
{
// Simple command batcher were commands like barriers
// can be stored and submitted at once
struct TaskCommandList : Graphics::CommandList
{
    // Memory barrier config
    constexpr static usize kMaxMemoryBarriers = 16;
    constexpr static usize kMaxImageBarriers = 16;
    // Descriptor buffer config

    TaskCommandList(Graphics::CommandList *list);
    ~TaskCommandList();
    void insert_memory_barrier(Graphics::MemoryBarrier &barrier);
    void insert_image_barrier(Graphics::ImageBarrier &barrier);
    void flush_barriers();

    TaskCommandList &set_pipeline(eastl::string_view pipeline_name);
    TaskCommandList &set_viewport(const glm::vec4 &viewport);
    TaskCommandList &debug_marker();

    usize m_memory_barrier_count = 0;
    eastl::array<Graphics::MemoryBarrier, kMaxImageBarriers> m_memory_barriers = {};
    usize m_image_barrier_count = 0;
    eastl::array<Graphics::ImageBarrier, kMaxImageBarriers> m_image_barriers = {};

    Graphics::CommandList *m_list = nullptr;
};

}  // namespace lr::Renderer