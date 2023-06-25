// Created on Friday November 18th 2022 by exdal
// Last modified on Sunday June 25th 2023 by exdal

#pragma once

namespace lr::Memory
{
using TLSFBlockID = u32;
struct TLSFBlock
{
    static constexpr TLSFBlockID kInvalid = ~0;

    u64 m_Offset : 63 = ~0;
    u64 m_IsFree : 1 = false;

    TLSFBlockID m_NextPhysical = kInvalid;
    TLSFBlockID m_PrevPhysical = kInvalid;

    TLSFBlockID m_PrevFree = kInvalid;
    TLSFBlockID m_NextFree = kInvalid;
};

struct TLSFAllocatorView
{
    static constexpr i32 ALIGN_SIZE_LOG2 = 3;
    static constexpr i32 ALIGN_SIZE = 1 << ALIGN_SIZE_LOG2;

    static constexpr i32 SL_INDEX_COUNT_LOG2 = 5;
    static constexpr i32 SL_INDEX_COUNT = 1 << SL_INDEX_COUNT_LOG2;

    static constexpr i32 FL_INDEX_MAX = 32;  // support up to 4 GB of allocation
    static constexpr i32 FL_INDEX_SHIFT = SL_INDEX_COUNT_LOG2 + ALIGN_SIZE_LOG2;
    static constexpr i32 FL_INDEX_COUNT = FL_INDEX_MAX - FL_INDEX_SHIFT + 1;
    static constexpr i32 FL_INDEX_SHIFT_SIZE = 1 << FL_INDEX_SHIFT;

    static constexpr u64 MIN_BLOCK_SIZE = 1 << FL_INDEX_SHIFT;

    /// ------------------------------------------------------------ ///

    void Init(u64 memSize, u32 maxAllocs);
    void Delete();

    TLSFBlockID Allocate(u64 size, u64 alignment = ALIGN_SIZE);
    void Free(TLSFBlockID blockID);

    TLSFBlockID FindFreeBlock(u64 size);
    TLSFBlockID AddFreeBlock(u64 size, u64 offset);
    void RemoveFreeBlock(TLSFBlockID blockID, bool removeBlock = true);

    // Bitmap utils
    u32 GetFirstIndex(u64 size);
    u32 GetSecondIndex(u32 firstIndex, u64 size);
    u32 GetFreeListIndex(u32 firstIndex, u32 secondIndex);
    u64 GetPhysicalSize(TLSFBlockID blockID);
    TLSFBlock *GetBlockData(TLSFBlockID blockID);

    u32 m_FirstListBitmap = 0;
    u32 m_pSecondListBitmap[FL_INDEX_COUNT] = {};

    TLSFBlock *m_pBlocks = nullptr;
    TLSFBlockID *m_pBlockIndices = nullptr;
    TLSFBlockID *m_pFreeBlocks = nullptr;
    u32 m_FreeListOffset = 0;

    u64 m_MaxSize = 0;
    u64 m_UsedSize = 0;
};

struct TLSFAllocatorDesc
{
    u64 m_DataSize = 0;
    u32 m_BlockCount = 0;
};
struct TLSFAllocator
{
    void Init(const TLSFAllocatorDesc &desc);
    void Delete();
    bool CanAllocate(u64 size, u32 alignment = 1);
    void *Allocate(u64 size, u32 alignment = 1, void **ppAllocatorData = nullptr);
    void Free(TLSFBlockID blockID, bool freeData);

    TLSFAllocatorView m_View;
    u8 *m_pData = nullptr;
};

}  // namespace lr::Memory