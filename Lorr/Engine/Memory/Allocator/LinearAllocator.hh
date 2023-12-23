#pragma once

namespace lr::Memory
{
struct LinearAllocatorView
{
    u64 allocate(u64 size, u64 alignment);
    void reset();

    u64 m_offset = 0;
};

}  // namespace lr::Memory