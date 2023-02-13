//
// Created on Friday 18th November 2022 by e-erdal
//

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
        void Init(AllocatorDesc &desc) override;
        void Delete() override;
        bool CanAllocate(u64 size, u32 alignment) override;
        bool Allocate(AllocationInfo &info) override;
        void Free(void *pData, bool freeData) override;
        void Grow(u64 size) override;
    };

}  // namespace lr::Memory