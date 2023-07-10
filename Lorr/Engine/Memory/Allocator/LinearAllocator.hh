// Created on Friday November 18th 2022 by exdal
// Last modified on Friday July 7th 2023 by exdal

#pragma once

#include <EASTL/atomic.h>

#include "Memory/MemoryUtils.hh"

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

    AllocationRegion *FindFreeRegion(u64 size);
    AllocationRegion *m_pFirstRegion = nullptr;
};

struct LinearAllocatorDesc
{
    u64 m_DataSize : 63 = 0;
    u64 m_AllowMultipleBlocks : 1 = 0;
};

struct LinearAllocator
{
    void Init(const LinearAllocatorDesc &desc);
    AllocationRegion *AllocateRegion();
    AllocationRegion *GetFirstRegion() { return m_View.m_pFirstRegion; }

    void Delete();
    bool CanAllocate(u64 size, u32 alignment = 1);
    void *Allocate(u64 size, u32 alignment = 1);
    void Reset();

    LinearAllocatorView m_View = {};
    u64 m_RegionSize : 63 = 0;
    u64 m_AllowMultipleBlocks : 1 = 0;
};

}  // namespace lr::Memory