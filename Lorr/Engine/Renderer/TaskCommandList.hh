#pragma once

#include "Graphics/CommandList.hh"

namespace lr::Renderer
{
// Simple command batcher were commands like barriers
// can be stored and submitted at once
struct TaskCommandList
{
    constexpr static usize kMaxMemoryBarriers = 16;
    constexpr static usize kMaxImageBarriers = 16;

    TaskCommandList(Graphics::CommandList *pList);
    ~TaskCommandList();
    void InsertMemoryBarrier(Graphics::MemoryBarrier &barrier);
    void InsertImageBarrier(Graphics::ImageBarrier &barrier);
    void FlushBarriers();

    usize m_MemoryBarrierCount = 0;
    eastl::array<Graphics::MemoryBarrier, kMaxImageBarriers> m_MemoryBarriers = {};
    usize m_ImageBarrierCount = 0;
    eastl::array<Graphics::ImageBarrier, kMaxImageBarriers> m_ImageBarriers = {};
    Graphics::CommandList *m_pList = nullptr;
};

}  // namespace lr::Renderer