// Created on Friday November 18th 2022 by exdal
// Last modified on Tuesday May 16th 2023 by exdal

#pragma once

namespace lr::Memory
{
//? If `pInitialData` is not `nullptr`, allocator will take it as a view.
//? When deleting the allocator, it wont delete `pInitialData`.
struct AllocatorDesc
{
    // Generic Info
    u64 m_DataSize = 0;
    void *m_pInitialData = nullptr;
    u32 m_AutoGrowSize = 0;  // 0 disables autogrow

    // TLSF Allocator Info
    u64 m_BlockCount = 0;
};

template<typename _View>
struct Allocator
{
    virtual void Init(const AllocatorDesc &desc) = 0;
    virtual void Delete() = 0;
    virtual bool CanAllocate(u64 size, u32 alignment = 1) = 0;
    virtual void *Allocate(u64 size, u32 alignment = 1, void **ppAllocatorData = nullptr) = 0;
    virtual void Free(void *pData = nullptr, bool freeData = false) = 0;

    _View m_View;
    u8 *m_pData = nullptr;
    bool m_SelfAllocated = false;
    u32 m_AutoGrowSize = 0;
};

}  // namespace lr::Memory