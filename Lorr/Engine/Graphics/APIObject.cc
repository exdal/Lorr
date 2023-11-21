#include "APIObject.hh"

namespace lr::Graphics
{
APIAllocator APIAllocator::m_g_handle;

void *APIAllocator::allocate_type(u64 size)
{
    Memory::TLSFBlockID blockID = m_type_allocator.Allocate(
        size + Memory::TLSFAllocatorView::ALIGN_SIZE, Memory::TLSFAllocatorView::ALIGN_SIZE);

    u64 offset = m_type_allocator.GetBlockData(blockID)->m_Offset;
    memcpy(m_type_data + offset, &blockID, sizeof(Memory::TLSFBlockID));
    return (m_type_data + offset + Memory::TLSFAllocatorView::ALIGN_SIZE);
}

void APIAllocator::free_type(void *pBlockAddr)
{
    Memory::TLSFBlockID blockID =
        *(Memory::TLSFBlockID *)((u8 *)pBlockAddr - Memory::TLSFAllocatorView::ALIGN_SIZE);
    m_type_allocator.Free(blockID);
}
}  // namespace lr::Graphics