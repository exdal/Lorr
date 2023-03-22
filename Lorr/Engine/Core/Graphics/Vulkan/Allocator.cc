#include "Allocator.hh"

namespace lr::Graphics
{
APIAllocator APIAllocator::g_Handle;

void *APIAllocator::Allocate(u64 size)
{
    Memory::TLSFBlock *pBlock =
        m_TypeAllocator.Allocate(size + PTR_SIZE, Memory::TLSFAllocatorView::ALIGN_SIZE);

    memcpy(m_pTypeData + pBlock->m_Offset, &pBlock, PTR_SIZE);
    return (m_pTypeData + pBlock->m_Offset + PTR_SIZE);
}

void APIAllocator::Free(void *pBlockAddr)
{
    Memory::TLSFBlock *pBlock = *(Memory::TLSFBlock **)((u8 *)pBlockAddr - PTR_SIZE);
    m_TypeAllocator.Free(pBlock);
}
}  // namespace lr::Graphics