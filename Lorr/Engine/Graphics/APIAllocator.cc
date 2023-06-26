// Created on Tuesday February 28th 2023 by exdal
// Last modified on Sunday June 25th 2023 by exdal

#include "APIAllocator.hh"

namespace lr::Graphics
{
APIAllocator APIAllocator::g_Handle;

void *APIAllocator::Allocate(u64 size)
{
    Memory::TLSFBlockID blockID = m_TypeAllocator.Allocate(
        size + Memory::TLSFAllocatorView::ALIGN_SIZE, Memory::TLSFAllocatorView::ALIGN_SIZE);

    u64 offset = m_TypeAllocator.GetBlockData(blockID)->m_Offset;
    memcpy(m_pTypeData + offset, &blockID, sizeof(Memory::TLSFBlockID));
    return (m_pTypeData + offset + Memory::TLSFAllocatorView::ALIGN_SIZE);
}

void APIAllocator::Free(void *pBlockAddr)
{
    Memory::TLSFBlockID blockID =
        *(Memory::TLSFBlockID *)((u8 *)pBlockAddr - Memory::TLSFAllocatorView::ALIGN_SIZE);
    m_TypeAllocator.Free(blockID);
}
}  // namespace lr::Graphics