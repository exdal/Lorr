#include "AreaAllocator.hh"

#include "Memory/MemoryUtils.hh"

namespace lr::Memory
{
u64 AllocationArea::consume_as_offset(u64 size)
{
    ZoneScoped;

    assert(size < m_capacity - m_size);
    if (size > m_capacity - m_size)
        return ~0;

    u64 offset = m_size;
    m_size += size;
    return offset;
}

u8 *AllocationArea::consume_as_data(u64 size)
{
    ZoneScoped;

    return m_data + consume_as_offset(size);
}

bool AreaAllocatorView::can_allocate(u64 size, u64 alignment)
{
    ZoneScoped;

    u64 alignedSize = align_up(size, alignment);
    return find_free_area(alignedSize) != nullptr;
}

void AreaAllocatorView::reset()
{
    ZoneScoped;

    AllocationArea *pCurrentRegion = m_first_area;
    while (pCurrentRegion != nullptr)
    {
        pCurrentRegion->m_size = 0;
        pCurrentRegion = pCurrentRegion->m_next;
    }
}

u64 AreaAllocatorView::size()
{
    ZoneScoped;

    u64 size = 0;
    AllocationArea *pCurrentRegion = m_first_area;
    while (pCurrentRegion != nullptr)
    {
        size += pCurrentRegion->m_size;
        pCurrentRegion = pCurrentRegion->m_next;
    }

    return size;
}

AllocationArea *AreaAllocatorView::find_free_area(u64 size)
{
    ZoneScoped;

    AllocationArea *pCurrentRegion = m_first_area;
    while (pCurrentRegion != nullptr)
    {
        if (pCurrentRegion->m_capacity - pCurrentRegion->m_size >= size)
            break;

        pCurrentRegion = pCurrentRegion->m_next;
    }

    return pCurrentRegion;
}

void AreaAllocator::init(u64 data_size)
{
    m_region_size = data_size;
    m_view.m_first_area = allocate_area(m_region_size);
}

AreaAllocator::~AreaAllocator()
{
    AllocationArea *pCurrentRegion = m_view.m_first_area;
    while (pCurrentRegion != nullptr)
    {
        AllocationArea *pNextRegion = pCurrentRegion->m_next;
        Memory::Release(pCurrentRegion);
        pCurrentRegion = pNextRegion;
    }
}

AllocationArea *AreaAllocator::allocate_area(usize size)
{
    ZoneScoped;

    u8 *pData = Memory::Allocate<u8>(sizeof(AllocationArea) + size);

    AllocationArea *pLastRegion = m_view.m_first_area;
    while (pLastRegion != nullptr && pLastRegion->m_next != nullptr)
        pLastRegion = pLastRegion->m_next;

    AllocationArea *pRegion = (AllocationArea *)pData;
    pRegion->m_next = nullptr;
    pRegion->m_capacity = size;
    pRegion->m_size = 0;
    pRegion->m_data = pData + sizeof(AllocationArea);
    if (pLastRegion)
        pLastRegion->m_next = pRegion;

    return pRegion;
}

bool AreaAllocator::can_allocate(u64 size, u64 alignment)
{
    ZoneScoped;

    return m_view.can_allocate(size, alignment);
}

u8 *AreaAllocator::allocate(u64 size, u64 alignment)
{
    ZoneScoped;

    u64 alignedSize = align_up(size, alignment);
    AllocationArea *pAvailRegion = m_view.find_free_area(alignedSize);
    if (!pAvailRegion)
    {
        m_region_size = eastl::max(m_region_size, alignedSize);
        pAvailRegion = allocate_area(m_region_size);
    }

    return pAvailRegion->consume_as_data(alignedSize);
}

}  // namespace lr::Memory