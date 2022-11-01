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

    void TLSFMemoryAllocator::Init(u64 memSize)
    {
        ZoneScoped;

        m_Size = memSize;

        m_BlockCount = 0x1000;
        m_pBlockPool = Memory::Allocate<TLSFBlock>(m_BlockCount);

        m_pFrontBlock = m_pBlockPool;
        m_pBackBlock = m_pBlockPool + m_BlockCount;

        /// CREATE INITIAL BLOCK ///

        TLSFBlock *pHeadBlock = AllocateInternalBlock();
        pHeadBlock->pPrevPhysical = pHeadBlock;
        pHeadBlock->pNextPhysical = nullptr;
        pHeadBlock->Offset = 0;

        AddFreeBlock(pHeadBlock);
    }

    TLSFBlock *TLSFMemoryAllocator::Allocate(u64 size, u32 alignment)
    {
        TLSFBlock *pAvailableBlock = nullptr;

        u64 alignedSize = AlignUp(size, ALIGN_SIZE);

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

            pAvailableBlock = pBlock;

            // u32 alignedOffset = (pBlock->Offset + (alignment - 1)) & ~(alignment - 1);
        }

        return pAvailableBlock;
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

    i32 TLSFMemoryAllocator::GetFirstIndex(u64 size)
    {
        if (size < MIN_BLOCK_SIZE)
            return 0;

        return GetMSB(size) - (FL_INDEX_SHIFT - 1);
    }

    i32 TLSFMemoryAllocator::GetSecondIndex(i32 firstIndex, u32 size)
    {
        if (size < MIN_BLOCK_SIZE)
            return size / (MIN_BLOCK_SIZE / SL_INDEX_COUNT);

        return (size >> ((firstIndex + (FL_INDEX_SHIFT - 1)) - SL_INDEX_COUNT_LOG2)) ^ (1 << SL_INDEX_COUNT_LOG2);
    }

    void TLSFMemoryAllocator::AddFreeBlock(TLSFBlock *pBlock)
    {
        ZoneScoped;

        u32 size = GetPhysicalSize(pBlock);

        u32 firstIndex = GetFirstIndex(size);
        u32 secondIndex = GetSecondIndex(firstIndex, size);

        TLSFBlock *pIndexBlock = m_ppBlocks[firstIndex][secondIndex];

        pBlock->pNextFree = pIndexBlock;
        pBlock->pPrevFree = nullptr;
        pBlock->IsFree = true;

        if (pIndexBlock)
            pIndexBlock->pPrevFree = pBlock;

        m_ppBlocks[firstIndex][secondIndex] = pBlock;

        m_FirstListBitmap |= 1 << firstIndex;
        m_pSecondListBitmap[firstIndex] |= 1 << secondIndex;
    }

    TLSFBlock *TLSFMemoryAllocator::FindFreeBlock(u32 size)
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
                return nullptr;  // well fuck

            firstIndex = GetLSB(firstListMap);
            secondListMap = m_pSecondListBitmap[firstIndex];
        }

        secondIndex = GetLSB(secondListMap);

        return m_ppBlocks[firstIndex][secondIndex];
    }

    void TLSFMemoryAllocator::RemoveFreeBlock(TLSFBlock *pBlock)
    {
        ZoneScoped;

        u32 size = GetPhysicalSize(pBlock);

        u32 firstIndex = GetFirstIndex(size);
        u32 secondIndex = GetSecondIndex(firstIndex, size);

        TLSFBlock *pPrevBlock = pBlock->pPrevFree;
        TLSFBlock *pNextBlock = pBlock->pNextFree;

        if (pNextBlock)
            pNextBlock->pPrevFree = pPrevBlock;

        if (pPrevBlock)
            pPrevBlock->pNextFree = pNextBlock;

        // pBlock->IsFree = false;

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

        if (blockSize >= size + MIN_BLOCK_SIZE)
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

    u32 TLSFMemoryAllocator::GetPhysicalSize(TLSFBlock *pBlock)
    {
        ZoneScoped;

        return pBlock->pNextPhysical ? pBlock->pNextPhysical->Offset - pBlock->Offset : m_Size - pBlock->Offset;
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

        return --m_pBackBlock;
    }

    void TLSFMemoryAllocator::FreeInternalBlock(TLSFBlock *pBlock)
    {
        ZoneScoped;

        if (pBlock == m_pBackBlock)
        {
            m_pBackBlock++;
        }
        else
        {
            pBlock->pNextPhysical = m_pFrontBlock;
            m_pFrontBlock = pBlock;
        }
    }

}  // namespace lr::Memory