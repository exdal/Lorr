//
// Created on Friday 18th November 2022 by e-erdal
//

#pragma once

namespace lr::Memory
{
    //? If `pInitialData` is not `nullptr`, allocator will take it as a view.
    //? When deleting the allocator, it wont delete `pInitialData`. 
    struct AllocatorDesc
    {
        // Generic Info
        u64 DataSize = 0;
        void *pInitialData = nullptr;

        // TLSF Allocator Info
        u64 BlockCount = 0;
    };

    // If `pData` is set to `nullptr`, it will be set to allocated data.
    struct AllocationInfo
    {
        u64 Size = 0;
        u32 Alignment = 0;
        void *pData = nullptr;

        // Allocator data output, i.e, address of TLSF block
        void *pAllocatorData = nullptr;
    };

    template<typename _View>
    struct Allocator
    {
        virtual void Init(AllocatorDesc &desc) = 0;
        virtual void Delete() = 0;
        virtual bool CanAllocate(u64 size, u32 alignment) = 0;
        virtual bool Allocate(AllocationInfo &info) = 0;
        virtual void Free(void *pData = nullptr, bool freeData = false) = 0;

        _View m_View;
        u8 *m_pData = nullptr;
        bool m_SelfAllocated = false;
    };

}  // namespace lr::Memory