//
// Created on Friday 18th November 2022 by e-erdal
//

#pragma once

#include "BaseAllocator.hh"

namespace lr::Memory
{
    struct TLSFBlock
    {
        u64 m_Offset : 63;
        u64 m_IsFree : 1;

        TLSFBlock *m_pNextPhysical = nullptr;
        TLSFBlock *m_pPrevPhysical = nullptr;

        TLSFBlock *m_pPrevFree = nullptr;
        TLSFBlock *m_pNextFree = nullptr;
    };

    struct TLSFAllocatorView
    {
        static constexpr i32 ALIGN_SIZE_LOG2 = 3;
        static constexpr i32 ALIGN_SIZE = 1 << ALIGN_SIZE_LOG2;

        static constexpr i32 SL_INDEX_COUNT_LOG2 = 5;
        static constexpr i32 SL_INDEX_COUNT = 1 << SL_INDEX_COUNT_LOG2;

        static constexpr i32 FL_INDEX_MAX = 60;
        static constexpr i32 FL_INDEX_SHIFT = SL_INDEX_COUNT_LOG2 + ALIGN_SIZE_LOG2;
        static constexpr i32 FL_INDEX_COUNT = FL_INDEX_MAX - FL_INDEX_SHIFT + 1;
        static constexpr i32 FL_INDEX_SHIFT_SIZE = 1 << FL_INDEX_SHIFT;

        static constexpr u64 MIN_BLOCK_SIZE = 1 << FL_INDEX_SHIFT;

        /// ------------------------------------------------------------ ///

        void Init(u64 memSize, u32 blockCount);
        void Delete();

        bool CanAllocate(u64 size, u32 alignment);
        TLSFBlock *Allocate(u64 size, u32 alignment = ALIGN_SIZE);
        void Free(TLSFBlock *pBlock);

        void AddFreeBlock(TLSFBlock *pBlock);
        TLSFBlock *FindFreeBlock(u32 size);
        void RemoveFreeBlock(TLSFBlock *pBlock);

        TLSFBlock *SplitBlock(TLSFBlock *pBlock, u32 size);
        void MergeBlock(TLSFBlock *pTarget, TLSFBlock *pSource);

        u32 GetPhysicalSize(TLSFBlock *pBlock);

        TLSFBlock *AllocateInternalBlock();
        void FreeInternalBlock(TLSFBlock *pBlock);

        u64 m_Size = 0;

        // Bitmap utils
        i32 GetFirstIndex(u64 size);
        i32 GetSecondIndex(i32 firstIndex, u32 size);

        u32 m_FirstListBitmap = 0;
        u32 m_pSecondListBitmap[FL_INDEX_COUNT] = {};

        u32 m_BlockCount = 0;
        TLSFBlock *m_pBlockPool = nullptr;
        TLSFBlock *m_ppBlocks[FL_INDEX_COUNT][SL_INDEX_COUNT] = {};

        TLSFBlock *m_pFrontBlock = nullptr;
        TLSFBlock *m_pBackBlock = nullptr;
    };

    struct TLSFAllocator : Allocator<TLSFAllocatorView>
    {
        void Init(AllocatorDesc &desc) override;
        void Delete() override;
        bool CanAllocate(u64 size, u32 alignment) override;
        bool Allocate(AllocationInfo &info) override;
        void Free(void *pData, bool freeData) override;
    };

}  // namespace lr::Memory