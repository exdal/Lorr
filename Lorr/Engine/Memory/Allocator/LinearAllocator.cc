// Created on Friday November 18th 2022 by exdal
// Last modified on Monday August 7th 2023 by exdal

#include "LinearAllocator.hh"

#include "Core/BackTrace.hh"
#include "Memory/MemoryUtils.hh"

namespace lr::Memory
{
u64 AllocationRegion::ConsumeAsOffset(u64 size)
{
    ZoneScoped;

    assert(size < m_Capacity - m_Size);
    if (size > m_Capacity - m_Size)
        return ~0;

    u64 offset = m_Size;
    m_Size += size;
    return offset;
}

u8 *AllocationRegion::ConsumeAsData(u64 size)
{
    ZoneScoped;

    return m_pData + ConsumeAsOffset(size);
}

bool LinearAllocatorView::CanAllocate(u64 size, u32 alignment)
{
    ZoneScoped;

    u64 alignedSize = Memory::AlignUp(size, alignment);
    return FindFreeRegion(alignedSize) != nullptr;
}

void LinearAllocatorView::Reset()
{
    ZoneScoped;

    AllocationRegion *pCurrentRegion = m_pFirstRegion;
    while (pCurrentRegion != nullptr)
    {
        pCurrentRegion->m_Size = 0;
        pCurrentRegion = pCurrentRegion->m_pNext;
    }
}

u64 LinearAllocatorView::Size()
{
    ZoneScoped;

    u64 size = 0;
    AllocationRegion *pCurrentRegion = m_pFirstRegion;
    while (pCurrentRegion != nullptr)
    {
        size += pCurrentRegion->m_Size;
        pCurrentRegion = pCurrentRegion->m_pNext;
    }

    return size;
}

AllocationRegion *LinearAllocatorView::FindFreeRegion(u64 size)
{
    ZoneScoped;

    AllocationRegion *pCurrentRegion = m_pFirstRegion;
    while (pCurrentRegion != nullptr)
    {
        if (pCurrentRegion->m_Capacity - pCurrentRegion->m_Size >= size)
            break;

        pCurrentRegion = pCurrentRegion->m_pNext;
    }

    return pCurrentRegion;
}

void LinearAllocator::Init(const LinearAllocatorDesc &desc)
{
    ZoneScoped;

    m_RegionSize = desc.m_DataSize;
    m_View.m_pFirstRegion = AllocateRegion();
}

AllocationRegion *LinearAllocator::AllocateRegion()
{
    ZoneScoped;

    u8 *pData = Memory::Allocate<u8>(sizeof(AllocationRegion) + m_RegionSize);

    AllocationRegion *pLastRegion = m_View.m_pFirstRegion;
    while (pLastRegion != nullptr && pLastRegion->m_pNext)
        pLastRegion = pLastRegion->m_pNext;

    AllocationRegion *pRegion = (AllocationRegion *)pData;
    pRegion->m_pNext = pLastRegion;
    pRegion->m_Capacity = m_RegionSize;
    pRegion->m_Size = 0;
    pRegion->m_pData = pData + sizeof(AllocationRegion);

    return pRegion;
}

void LinearAllocator::Delete()
{
    ZoneScoped;

    AllocationRegion *pCurrentRegion = m_View.m_pFirstRegion;
    while (pCurrentRegion != nullptr)
    {
        AllocationRegion *pNextRegion = pCurrentRegion->m_pNext;
        Memory::Release(pCurrentRegion);
        pCurrentRegion = pNextRegion;
    }
}

bool LinearAllocator::CanAllocate(u64 size, u32 alignment)
{
    ZoneScoped;

    return m_View.CanAllocate(size, alignment);
}

u8 *LinearAllocator::Allocate(u64 size, u32 alignment)
{
    ZoneScoped;

    u64 alignedSize = Memory::AlignUp(size, alignment);
    AllocationRegion *pAvailRegion = m_View.FindFreeRegion(alignedSize);
    if (!pAvailRegion)
    {
        if (!m_AllowMultipleBlocks)
            return nullptr;

        pAvailRegion = AllocateRegion();
    }

    return pAvailRegion->ConsumeAsData(alignedSize);
}

}  // namespace lr::Memory