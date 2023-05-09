#include "TLSFAllocator.hh"

#include "Memory/MemoryUtils.hh"

namespace lr::Memory
{
void TLSFAllocatorView::Init(u64 memSize, u32 blockCount)
{
    ZoneScoped;

    m_Size = memSize;

    m_BlockCount = blockCount;
    m_pBlockPool = Memory::Allocate<TLSFBlock>(m_BlockCount);

    m_pFrontBlock = m_pBlockPool;
    m_pBackBlock = m_pBlockPool + m_BlockCount;

    /// CREATE INITIAL BLOCK ///

    TLSFBlock *pHeadBlock = AllocateInternalBlock();
    pHeadBlock->m_pPrevPhysical = pHeadBlock;
    pHeadBlock->m_pNextPhysical = nullptr;
    pHeadBlock->m_Offset = 0;

    AddFreeBlock(pHeadBlock);
}

void TLSFAllocatorView::Delete()
{
    ZoneScoped;

    Memory::Release(m_pBlockPool);
}

bool TLSFAllocatorView::CanAllocate(u64 size, u32 alignment)
{
    ZoneScoped;

    u64 alignedSize = Memory::AlignUp(size, ALIGN_SIZE);
    TLSFBlock *pBlock = FindFreeBlock(alignedSize);

    if (!pBlock)
        return false;

    if (!pBlock->m_IsFree && GetPhysicalSize(pBlock) < size)
        return false;

    return true;
}

TLSFBlock *TLSFAllocatorView::Allocate(u64 size, u32 alignment)
{
    TLSFBlock *pAvailableBlock = nullptr;

    u64 alignedSize = Memory::AlignUp(size, alignment);

    /// Find free and closest block available
    TLSFBlock *pBlock = FindFreeBlock(alignedSize);

    if (pBlock)
    {
        assert(pBlock->m_IsFree && "The block found is not free.");
        assert(GetPhysicalSize(pBlock) >= size && "Found block doesn't match with the aligned size.");

        RemoveFreeBlock(pBlock);

        TLSFBlock *pSplitBlock = SplitBlock(pBlock, size);
        if (pSplitBlock)
        {
            AddFreeBlock(pSplitBlock);
        }

        pAvailableBlock = pBlock;
    }

    return pAvailableBlock;
}

void TLSFAllocatorView::Free(TLSFBlock *pBlock)
{
    ZoneScoped;

    TLSFBlock *pPrev = pBlock->m_pPrevPhysical;
    TLSFBlock *pNext = pBlock->m_pNextPhysical;

    if (pPrev && pPrev->m_IsFree)
    {
        RemoveFreeBlock(pPrev);
        MergeBlock(pPrev, pBlock);

        pBlock = pPrev;
    }

    if (pNext && pNext->m_IsFree)
    {
        RemoveFreeBlock(pNext);
        MergeBlock(pBlock, pNext);
    }

    AddFreeBlock(pBlock);
}

i32 TLSFAllocatorView::GetFirstIndex(u64 size)
{
    if (size < MIN_BLOCK_SIZE)
        return 0;

    return GetMSB(size) - (FL_INDEX_SHIFT - 1);
}

i32 TLSFAllocatorView::GetSecondIndex(i32 firstIndex, u32 size)
{
    if (size < MIN_BLOCK_SIZE)
        return size / (MIN_BLOCK_SIZE / SL_INDEX_COUNT);

    return (size >> ((firstIndex + (FL_INDEX_SHIFT - 1)) - SL_INDEX_COUNT_LOG2)) ^ (1 << SL_INDEX_COUNT_LOG2);
}

void TLSFAllocatorView::AddFreeBlock(TLSFBlock *pBlock)
{
    ZoneScoped;

    u32 size = GetPhysicalSize(pBlock);

    u32 firstIndex = GetFirstIndex(size);
    u32 secondIndex = GetSecondIndex(firstIndex, size);

    TLSFBlock *pIndexBlock = m_ppBlocks[firstIndex][secondIndex];

    pBlock->m_pNextFree = pIndexBlock;
    pBlock->m_pPrevFree = nullptr;
    pBlock->m_IsFree = true;

    if (pIndexBlock)
        pIndexBlock->m_pPrevFree = pBlock;

    m_ppBlocks[firstIndex][secondIndex] = pBlock;

    m_FirstListBitmap |= 1 << firstIndex;
    m_pSecondListBitmap[firstIndex] |= 1 << secondIndex;
}

TLSFBlock *TLSFAllocatorView::FindFreeBlock(u32 size)
{
    ZoneScoped;

    u32 firstIndex = GetFirstIndex(size);
    u32 secondIndex = GetSecondIndex(firstIndex, size);

    u32 secondListMap = m_pSecondListBitmap[firstIndex] & (~0U << secondIndex);

    if (secondListMap == 0)
    {
        // Could not find any available block in that index, so go higher
        u32 firstListMap = m_FirstListBitmap & (~0U << (firstIndex + 1));
        if (firstListMap == 0)
        {
            LOG_ERROR(
                "Failed to allocate block, FL map is zero. "
                "({} bytes, FI: {} SI: {} SL map: {}, FL map: {})",
                size,
                firstIndex,
                secondIndex,
                firstListMap,
                secondListMap);

            return nullptr;
        }

        firstIndex = GetLSB(firstListMap);
        secondListMap = m_pSecondListBitmap[firstIndex];
    }

    secondIndex = GetLSB(secondListMap);

    return m_ppBlocks[firstIndex][secondIndex];
}

void TLSFAllocatorView::RemoveFreeBlock(TLSFBlock *pBlock)
{
    ZoneScoped;

    u32 size = GetPhysicalSize(pBlock);

    u32 firstIndex = GetFirstIndex(size);
    u32 secondIndex = GetSecondIndex(firstIndex, size);

    TLSFBlock *pPrevBlock = pBlock->m_pPrevFree;
    TLSFBlock *pNextBlock = pBlock->m_pNextFree;

    if (pNextBlock)
        pNextBlock->m_pPrevFree = pPrevBlock;

    if (pPrevBlock)
        pPrevBlock->m_pNextFree = pNextBlock;

    pBlock->m_IsFree = false;

    TLSFBlock *pListBlock = m_ppBlocks[firstIndex][secondIndex];

    if (pListBlock == pBlock)
    {
        m_ppBlocks[firstIndex][secondIndex] = pNextBlock;

        if (pNextBlock == nullptr)
        {
            m_pSecondListBitmap[firstIndex] &= ~(1 << secondIndex);

            if (m_pSecondListBitmap[firstIndex] == 0)
            {
                m_FirstListBitmap &= ~(1 << firstIndex);
            }
        }
    }
}

TLSFBlock *TLSFAllocatorView::SplitBlock(TLSFBlock *pBlock, u32 size)
{
    ZoneScoped;

    u32 blockSize = GetPhysicalSize(pBlock);
    TLSFBlock *pNewBlock = nullptr;

    if (blockSize >= size + MIN_BLOCK_SIZE)
    {
        pNewBlock = AllocateInternalBlock();

        TLSFBlock *pNextBlock = pBlock->m_pNextPhysical;
        u64 offset = pBlock->m_Offset;

        pBlock->m_pNextPhysical = pNewBlock;

        pNewBlock->m_Offset = offset + size;
        pNewBlock->m_pNextPhysical = pNextBlock;
        pNewBlock->m_pPrevPhysical = pBlock;

        if (pNextBlock)
            pNextBlock->m_pPrevPhysical = pNewBlock;
    }

    return pNewBlock;
}

void TLSFAllocatorView::MergeBlock(TLSFBlock *pTarget, TLSFBlock *pSource)
{
    ZoneScoped;

    TLSFBlock *pNextSource = pSource->m_pNextPhysical;
    pTarget->m_pNextPhysical = pNextSource;

    if (pNextSource)
        pNextSource->m_pPrevPhysical = pTarget;

    FreeInternalBlock(pSource);
}

u32 TLSFAllocatorView::GetPhysicalSize(TLSFBlock *pBlock)
{
    ZoneScoped;

    return pBlock->m_pNextPhysical ? pBlock->m_pNextPhysical->m_Offset - pBlock->m_Offset
                                   : m_Size - pBlock->m_Offset;
}

//! THIS IS SO BAD THIS IS SO BAD THIS IS SO BAD THIS IS SO BAD THIS IS SO BAD THIS IS SO BAD THIS IS SO BAD
//! THIS IS SO BAD THIS IS SO BAD THIS IS SO BAD THIS IS SO BAD THIS IS SO BAD THIS IS SO BAD THIS IS SO BAD
//! THIS IS SO BAD THIS IS SO BAD THIS IS SO BAD THIS IS SO BAD THIS IS SO BAD THIS IS SO BAD THIS IS SO BAD
//! THIS IS SO BAD THIS IS SO BAD THIS IS SO BAD THIS IS SO BAD THIS IS SO BAD THIS IS SO BAD THIS IS SO BAD
//! THIS IS SO BAD THIS IS SO BAD THIS IS SO BAD THIS IS SO BAD THIS IS SO BAD THIS IS SO BAD THIS IS SO BAD
//! THIS IS SO BAD THIS IS SO BAD THIS IS SO BAD THIS IS SO BAD THIS IS SO BAD THIS IS SO BAD THIS IS SO BAD
TLSFBlock *TLSFAllocatorView::AllocateInternalBlock()
{
    ZoneScoped;

    if (m_pFrontBlock)
    {
        TLSFBlock *pBlock = m_pFrontBlock;
        m_pFrontBlock = pBlock->m_pNextPhysical;

        return pBlock;
    }

    return --m_pBackBlock;
}

void TLSFAllocatorView::FreeInternalBlock(TLSFBlock *pBlock)
{
    ZoneScoped;

    if (pBlock == m_pBackBlock)
    {
        m_pBackBlock++;
    }
    else
    {
        pBlock->m_pNextPhysical = m_pFrontBlock;
        m_pFrontBlock = pBlock;
    }
}

void TLSFAllocator::Init(const AllocatorDesc &desc)
{
    ZoneScoped;

    m_View.Init(desc.m_DataSize, desc.m_BlockCount);
    m_pData = (u8 *)desc.m_pInitialData;
    m_SelfAllocated = !(bool)m_pData;

    if (!m_pData)
        m_pData = Memory::Allocate<u8>(desc.m_DataSize);
}

void TLSFAllocator::Delete()
{
    ZoneScoped;

    m_View.Delete();

    if (m_SelfAllocated)
        Memory::Release(m_pData);
}

bool TLSFAllocator::CanAllocate(u64 size, u32 alignment)
{
    ZoneScoped;

    return m_View.CanAllocate(size, alignment);
}

void *TLSFAllocator::Allocate(u64 size, u32 alignment, void **ppAllocatorData)
{
    ZoneScoped;

    TLSFBlock *pBlock = m_View.Allocate(size, alignment);
    if (!pBlock)
        return nullptr;

    *ppAllocatorData = pBlock;
    return m_pData + pBlock->m_Offset;
}

void TLSFAllocator::Free(void *pData, bool freeData)
{
    ZoneScoped;

    TLSFBlock *pBlock = (TLSFBlock *)pData;
    m_View.Free(pBlock);

    if (freeData)
        Memory::Release(m_pData);
}

}  // namespace lr::Memory