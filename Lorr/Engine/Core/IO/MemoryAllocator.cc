#include "MemoryAllocator.hh"

#include "Memory.hh"

namespace lr::Memory
{
    u32 LinearMemoryAllocator::AddPool(u64 size)
    {
        ZoneScoped;

        Pool &pool = m_pPools[m_PoolCount];
        pool.m_Size = size;

        return m_PoolCount++;
    }

    u64 LinearMemoryAllocator::Allocate(u32 poolIndex, u64 size, u32 alignment)
    {
        ZoneScoped;

        Pool &pool = m_pPools[poolIndex];

        u32 alignedSize = (size + alignment - 1) & ~(alignment - 1);

        if (pool.m_Offset + alignedSize > pool.m_Size)
        {
            LOG_ERROR("Linear memory allocator reached it's limit. Allocated {} and requested {}.", pool.m_Size, alignedSize);
            return -1;
        }

        u64 off = pool.m_Offset;
        pool.m_Offset += alignedSize;

        return off;
    }

    void LinearMemoryAllocator::Free(u32 poolIndex)
    {
        ZoneScoped;

        Pool &pool = m_pPools[poolIndex];

        pool.m_Offset = 0;
    }

    /// ------------------------------------------------------ ///

    void TLSFMemoryAllocator::Init(u32 poolSize)
    {
        ZoneScoped;

        m_Size = poolSize;

        m_BlockCount = BLOCK_ALLOC_COUNT;
        m_pBlockPool = Memory::Allocate<TLSFBlock>(m_BlockCount);

        m_pFrontBlock = m_pBlockPool;
        m_pBackBlock = m_pBlockPool + m_BlockCount;

        /// CREATE INITIAL BLOCK ///

        TLSFBlock *pHeadBlock = AllocateInternalBlock();
        pHeadBlock->pPrevPhysical = nullptr;
        pHeadBlock->pNextPhysical = nullptr;
        pHeadBlock->Offset = 0;

        AddFreeBlock(pHeadBlock);
    }

    u32 TLSFMemoryAllocator::Allocate(u32 size, u32 alignment, TLSFBlock *&pBlockOut)
    {
        ZoneScoped;

        TLSFBlock *pAvailableBlock = nullptr;

        u32 alignedSize = (size + ALIGN_SIZE_MASK - 1) & ~(ALIGN_SIZE_MASK - 1);

        /// Find free and closest block available
        TLSFBlock *pBlock = FindFreeBlock(alignedSize);

        if (pBlock)
        {
            assert(pBlock->IsFree && "The block found is not free.");
            assert(GetPhysicalSize(pBlock) >= size && "Found block doesn't match with the aligned size.");

            RemoveFreeBlock(pBlock);

            TLSFBlock *pSplitBlock = SplitBlock(pBlock, size);

            if (pSplitBlock)
            {
                AddFreeBlock(pSplitBlock);
            }

            pBlockOut = pBlock;
            
            u32 alignedOffset = (pBlock->Offset + alignment - 1) & ~(alignment - 1);
            return alignedOffset;
        }

        return 0;
    }

    void TLSFMemoryAllocator::Free(TLSFBlock *pBlock)
    {
        ZoneScoped;

        TLSFBlock *pPrev = pBlock->pPrevPhysical;
        TLSFBlock *pNext = pBlock->pNextPhysical;

        if (pPrev && pPrev->IsFree)
        {
            RemoveFreeBlock(pPrev);
            MergeBlock(pPrev, pBlock);

            pBlock = pPrev;
        }

        if (pNext && pNext->IsFree)
        {
            RemoveFreeBlock(pNext);
            MergeBlock(pBlock, pNext);
        }

        AddFreeBlock(pBlock);
    }

    void TLSFMemoryAllocator::AddFreeBlock(TLSFBlock *pBlock)
    {
        ZoneScoped;

        u32 size = GetPhysicalSize(pBlock);

        u32 firstIndex = 0;
        u32 secondIndex = 0;
        GetListIndex(size, firstIndex, secondIndex, false);

        TLSFBlock *pNextBlock = m_ppBlocks[firstIndex][secondIndex];

        if (pNextBlock)
        {
            pNextBlock->pPrevFree = pBlock;
        }

        pBlock->pPrevFree = nullptr;
        pBlock->pNextFree = pNextBlock;
        pBlock->IsFree = true;

        m_ppBlocks[firstIndex][secondIndex] = pBlock;

        m_FirstListBitmap |= 1 << firstIndex;
        m_pSecondListBitmap[firstIndex] |= 1 << secondIndex;
    }

    TLSFBlock *TLSFMemoryAllocator::FindFreeBlock(u32 size)
    {
        ZoneScoped;

        u32 firstIndex = 0;
        u32 secondIndex = 0;
        GetListIndex(size, firstIndex, secondIndex, true);

        u32 secondListMap = m_pSecondListBitmap[firstIndex] & (~0U << secondIndex);

        if (secondListMap == 0)
        {
            // Could not find any available block in that index, so go higher
            u32 firstListMap = m_FirstListBitmap & (~0U << (firstIndex + 1));
            if (firstListMap == 0)
                return nullptr;  // well fuck

            firstIndex = Memory::GetLSB(firstListMap);
            secondListMap = m_pSecondListBitmap[firstIndex];
        }

        secondIndex = Memory::GetLSB(secondListMap);

        return m_ppBlocks[firstIndex][secondIndex];
    }

    void TLSFMemoryAllocator::RemoveFreeBlock(TLSFBlock *pBlock)
    {
        ZoneScoped;

        u32 size = GetPhysicalSize(pBlock);

        u32 firstIndex = 0;
        u32 secondIndex = 0;
        GetListIndex(size, firstIndex, secondIndex, false);

        TLSFBlock *pPrevBlock = pBlock->pPrevFree;
        TLSFBlock *pNextBlock = pBlock->pNextFree;

        if (pNextBlock)
        {
            pNextBlock->pPrevFree = pPrevBlock;
        }

        if (pPrevBlock)
        {
            pPrevBlock->pNextFree = pNextBlock;
        }

        pBlock->IsFree = false;

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

    TLSFBlock *TLSFMemoryAllocator::SplitBlock(TLSFBlock *pBlock, u32 size)
    {
        ZoneScoped;

        u32 blockSize = GetPhysicalSize(pBlock);
        TLSFBlock *pNewBlock = nullptr;

        if (blockSize >= size + SMALL_BLOCK_SIZE)
        {
            pNewBlock = AllocateInternalBlock();

            TLSFBlock *pNextBlock = pBlock->pNextPhysical;
            u64 offset = pBlock->Offset;

            pBlock->pNextPhysical = pNewBlock;

            pNewBlock->Offset = offset + size;
            pNewBlock->pNextPhysical = pNextBlock;
            pNewBlock->pPrevPhysical = pBlock;

            if (pNextBlock)
                pNextBlock->pPrevPhysical = pNewBlock;
        }

        return pNewBlock;
    }

    void TLSFMemoryAllocator::MergeBlock(TLSFBlock *pTarget, TLSFBlock *pSource)
    {
        ZoneScoped;

        TLSFBlock *pNextSource = pSource->pNextPhysical;
        pTarget->pNextPhysical = pNextSource;
        pNextSource->pPrevPhysical = pTarget;

        FreeInternalBlock(pSource);
    }

    void TLSFMemoryAllocator::GetListIndex(u32 size, u32 &firstIndex, u32 &secondIndex, bool aligned)
    {
        ZoneScoped;

        u32 alignMask = 1;

        firstIndex = size < FL_INDEX_SHIFT_SIZE ? ALIGN_SIZE_LOG2 : Memory::GetMSB(size) - SL_INDEX_COUNT_LOG2;

        if (aligned)
        {
            //? Note: We subtract from 32 because of `u32`, don't forget to change this to `firstIndex` bit count.
            size += ~0U >> (32 - firstIndex);
            alignMask = 3;
        }

        size >>= firstIndex;
        secondIndex = size & (SL_INDEX_COUNT - 1);

        size >>= SL_INDEX_COUNT_LOG2;
        firstIndex = (firstIndex - ALIGN_SIZE_LOG2) + (size & alignMask);
    }

    u32 TLSFMemoryAllocator::GetPhysicalSize(TLSFBlock *pBlock)
    {
        ZoneScoped;

        return pBlock->pNextPhysical ? pBlock->pNextPhysical->Offset : m_Size - pBlock->Offset;
    }

    TLSFBlock *TLSFMemoryAllocator::AllocateInternalBlock()
    {
        ZoneScoped;

        if (m_pFrontBlock)
        {
            TLSFBlock *pBlock = m_pFrontBlock;
            m_pFrontBlock = pBlock->pNextPhysical;

            return pBlock;
        }

        return ++m_pBackBlock;
    }

    void TLSFMemoryAllocator::FreeInternalBlock(TLSFBlock *pBlock)
    {
        ZoneScoped;

        if (pBlock == m_pBackBlock)
        {
            m_pBackBlock--;
        }
        else
        {
            pBlock->pNextPhysical = m_pFrontBlock;
            m_pFrontBlock = pBlock;
        }
    }

}  // namespace lr::Memory