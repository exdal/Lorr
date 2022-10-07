//
// Created on Monday 26th September 2022 by e-erdal
//

#pragma once

namespace lr::Memory
{
    struct LinearMemoryAllocator
    {
        struct Pool
        {
            u64 m_Offset = 0;
            u64 m_Size = 0;
        };

        u32 AddPool(u64 size);

        u64 Allocate(u32 pool, u64 size);
        void Free(u32 pool);

        u32 m_PoolCount = 0;
        Pool m_pPools[24];
    };

    /// ------------------------------------------------------------ //
    //*
    //* Slightly modified version of Mykhailo Parfeniuk 'ETLSF'
    //*

    struct TLSFBlock
    {
        u64 Offset = 0;
        bool IsFree = false;

        TLSFBlock *pNextPhysical = nullptr;
        TLSFBlock *pPrevPhysical = nullptr;

        TLSFBlock *pPrevFree = nullptr;
        TLSFBlock *pNextFree = nullptr;
    };

    struct TLSFMemoryAllocator
    {
        static constexpr i32 ALIGN_SIZE_LOG2 = 8;
        static constexpr i32 ALIGN_SIZE = 1 << ALIGN_SIZE_LOG2;
        static constexpr i32 ALIGN_SIZE_MASK = ALIGN_SIZE - 1;

        static constexpr i32 SL_INDEX_COUNT_LOG2 = 5;
        static constexpr i32 SL_INDEX_COUNT = 1 << SL_INDEX_COUNT_LOG2;

        static constexpr i32 FL_INDEX_MAX = 60;
        static constexpr i32 FL_INDEX_SHIFT = SL_INDEX_COUNT_LOG2 + ALIGN_SIZE_LOG2;
        static constexpr i32 FL_INDEX_COUNT = FL_INDEX_MAX - FL_INDEX_SHIFT + 1;
        static constexpr i32 FL_INDEX_SHIFT_SIZE = 1 << FL_INDEX_SHIFT;

        static constexpr i32 SMALL_BLOCK_SIZE = 256;

        static constexpr i32 BLOCK_ALLOC_COUNT = 0x20000;

        /// ------------------------------------------------------------ //

        void Init(u32 poolSize);

        TLSFBlock *Allocate(u32 size);
        void Deallocate(TLSFBlock *pBlock);

        void AddFreeBlock(TLSFBlock *pBlock);
        TLSFBlock *FindFreeBlock(u32 size);
        void RemoveFreeBlock(TLSFBlock *pBlock);

        TLSFBlock *SplitBlock(TLSFBlock *pBlock, u32 size);
        void MergeBlock(TLSFBlock *pTarget, TLSFBlock *pSource);

        void GetListIndex(u32 size, u32 &firstIndex, u32 &secondIndex, bool aligned);

        u32 GetPhysicalSize(TLSFBlock *pBlock);

        TLSFBlock *AllocateInternalBlock();
        void FreeInternalBlock(TLSFBlock *pBlock);

        u64 m_Size = 0;

        /// Bitmaps
        u32 m_FirstListBitmap = 0;
        u32 m_pSecondListBitmap[FL_INDEX_COUNT] = { 0 };

        /// Freelist
        TLSFBlock *m_ppBlocks[FL_INDEX_COUNT][SL_INDEX_COUNT] = { 0 };

        u32 m_BlockCount = 0;
        TLSFBlock *m_pFrontBlock = nullptr;
        TLSFBlock *m_pBackBlock = nullptr;
        TLSFBlock *m_pBlockPool = nullptr;
    };

}  // namespace lr::Memory