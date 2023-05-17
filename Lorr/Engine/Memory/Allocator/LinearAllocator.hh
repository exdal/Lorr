// Created on Friday November 18th 2022 by exdal
// Last modified on Tuesday May 16th 2023 by exdal

#pragma once

#include "BaseAllocator.hh"

namespace lr::Memory
{
struct LinearAllocatorView
{
    void Init(u64 size);

    bool CanAllocate(u64 size, u32 alignment);
    u64 Allocate(u64 size, u32 alignment);
    void Free();

    u64 m_Offset = 0;
    u64 m_Size = 0;
};

struct LinearAllocator : Allocator<LinearAllocatorView>
{
    void Init(const AllocatorDesc &desc) override;
    void Delete() override;
    bool CanAllocate(u64 size, u32 alignment = 1) override;
    void *Allocate(u64 size, u32 alignment = 1, void **ppAllocatorData = nullptr) override;
    void Free(void *pData, bool freeData) override;
    void Reserve(u64 size);
};

}  // namespace lr::Memory