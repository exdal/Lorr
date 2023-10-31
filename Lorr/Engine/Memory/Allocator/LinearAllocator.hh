#pragma once

#include <EASTL/atomic.h>

namespace lr::Memory
{
struct AllocationRegion
{
    u64 ConsumeAsOffset(u64 size);
    u8 *ConsumeAsData(u64 size);

    AllocationRegion *m_pNext = nullptr;
    u64 m_Capacity = 0;
    u64 m_Size = 0;
    u8 *m_pData = nullptr;  // Also user-data
};

struct LinearAllocatorView
{
    bool CanAllocate(u64 size, u32 alignment);
    void Reset();
    u64 Size();

    AllocationRegion *FindFreeRegion(u64 size);
    AllocationRegion *m_pFirstRegion = nullptr;
};

struct LinearAllocatorDesc
{
    u64 m_DataSize : 63 = 0;
    u64 m_AllowMultipleBlocks : 1 = 0;
};

struct LinearAllocator : LinearAllocatorView
{
    void Init(const LinearAllocatorDesc &desc);
    AllocationRegion *AllocateRegion(usize size);
    AllocationRegion *GetFirstRegion() { return m_View.m_pFirstRegion; }

    void Delete();
    bool CanAllocate(u64 size, u32 alignment = 1);
    u8 *Allocate(u64 size, u32 alignment = 1);
    template<typename _T, typename... _Args>
    _T *New(_Args &&...args);

    LinearAllocatorView m_View = {};
    u64 m_MinRegionSize : 63 = 0;
    u64 m_AllowMultipleBlocks : 1 = 0;
};

template<typename _T, typename... _Args>
_T *LinearAllocator::New(_Args &&...args)
{
    return new (Allocate(sizeof(_T), alignof(_T))) _T(eastl::forward<_Args>(args)...);
}

}  // namespace lr::Memory