// Created on Friday November 18th 2022 by exdal
// Last modified on Wednesday May 17th 2023 by exdal

#include "LinearAllocator.hh"

#include "Memory/MemoryUtils.hh"

namespace lr::Memory
{
void LinearAllocatorView::Init(u64 size)
{
    ZoneScoped;

    m_Size = size;
}

bool LinearAllocatorView::CanAllocate(u64 size, u32 alignment)
{
    ZoneScoped;

    u32 alignedSize = Memory::AlignUp(size, alignment);

    if (m_Offset + alignedSize > m_Size)
        return false;

    return true;
}

u64 LinearAllocatorView::Allocate(u64 size, u32 alignment)
{
    ZoneScoped;

    if (!CanAllocate(size, alignment))
    {
        LOG_ERROR(
            "Linear memory allocator reached it's limit. Allocated {} in total and requested {}.",
            m_Size,
            size);
        return -1;
    }

    u32 alignedSize = Memory::AlignUp(size, alignment);

    u64 off = m_Offset;
    m_Offset += alignedSize;

    return off;
}

void LinearAllocatorView::Grow(u64 size)
{
    ZoneScoped;

    m_Size += size;
}

void LinearAllocatorView::Free()
{
    ZoneScoped;

    m_Offset = 0;
}

/// THREAD SAFE ALLOCATORS ///

void LinearAllocatorViewAtomic::Init(u64 size)
{
    ZoneScoped;

    m_Size.store(size, eastl::memory_order_release);
}

bool LinearAllocatorViewAtomic::CanAllocate(u64 requestedSize, u32 alignment)
{
    ZoneScoped;

    u64 alignedSize = Memory::AlignUp(requestedSize, alignment);
    u64 offset = m_Offset.load(eastl::memory_order_acquire);
    u64 size = m_Size.load(eastl::memory_order_acquire);

    if (offset + alignedSize > size)
        return false;

    return true;
}

u64 LinearAllocatorViewAtomic::Allocate(u64 size, u32 alignment)
{
    ZoneScoped;

    if (!CanAllocate(size, alignment))
    {
        LOG_ERROR("Out of memory! Cannot allocate more. Req: {}", size);
        return -1;
    }

    u32 alignedSize = Memory::AlignUp(size, alignment);
    return m_Offset.fetch_add(alignedSize, eastl::memory_order_acq_rel);
}

void LinearAllocatorViewAtomic::Grow(u64 size)
{
    ZoneScoped;

    m_Size.add_fetch(size, eastl::memory_order_acq_rel);
}

void LinearAllocatorViewAtomic::Free()
{
    ZoneScoped;

    m_Offset.store(0, eastl::memory_order_release);
}


}  // namespace lr::Memory