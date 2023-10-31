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
    void *Allocate(TLSFBlockID &blockIDOut, u64 size, u32 alignment = 1);
    void Free(TLSFBlockID blockID, bool freeData);

    TLSFAllocatorView m_View;
    u8 *m_pData = nullptr;
};

// TODO
class STLAllocatorTLSF
{
public:
    EASTL_ALLOCATOR_EXPLICIT STLAllocatorTLSF(
        const char *pName = EASTL_NAME_VAL(EASTL_ALLOCATOR_DEFAULT_NAME));
    STLAllocatorTLSF(const STLAllocatorTLSF &x);
    STLAllocatorTLSF(const STLAllocatorTLSF &x, const char *pName);

    STLAllocatorTLSF &operator=(const STLAllocatorTLSF &x);

    void *allocate(size_t n, int flags = 0);
    void *allocate(size_t n, size_t alignment, size_t offset, int flags = 0);
    void deallocate(void *p, size_t n);

    const char *get_name() const;
    void set_name(const char *pName);

protected:
#if EASTL_NAME_ENABLED
    const char *mpName;  // Debug name, used to track memory.
#endif
};

bool operator==(const STLAllocatorTLSF &a, const STLAllocatorTLSF &b);
#if !defined(EA_COMPILER_HAS_THREE_WAY_COMPARISON)
bool operator!=(const STLAllocatorTLSF &a, const STLAllocatorTLSF &b);
#endif

}  // namespace lr::Memory