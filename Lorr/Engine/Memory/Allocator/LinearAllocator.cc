#include "LinearAllocator.hh"

namespace lr::Memory {
u64 LinearAllocatorView::allocate(u64 size, u64 alignment)
{
    ZoneScoped;

    u64 aligned_size = align_up(size, alignment);
    u64 offset = m_offset;
    m_offset += aligned_size;

    return offset;
}

void LinearAllocatorView::reset()
{
    ZoneScoped;

    m_offset = 0;
}
}  // namespace lr::Memory