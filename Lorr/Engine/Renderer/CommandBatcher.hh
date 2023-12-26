#pragma once

#include "Graphics/CommandList.hh"

namespace lr::Graphics
{
struct CommandBatcher
{
    constexpr static usize kMaxMemoryBarriers = 16;
    constexpr static usize kMaxImageBarriers = 16;

    CommandBatcher(CommandList *command_list);
    ~CommandBatcher();

    void insert_memory_barrier(MemoryBarrier &barrier);
    void insert_image_barrier(ImageBarrier &barrier);
    void flush_barriers();

    fixed_vector<MemoryBarrier, kMaxMemoryBarriers> m_memory_barriers = {};
    fixed_vector<ImageBarrier, kMaxImageBarriers> m_image_barriers = {};

    CommandList *m_command_list = nullptr;
};

}  // namespace lr::Graphics