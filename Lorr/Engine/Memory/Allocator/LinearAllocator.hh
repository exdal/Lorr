// Created on Friday November 18th 2022 by exdal
// Last modified on Monday June 12th 2023 by exdal

#pragma once

#include <EASTL/atomic.h>

#include "Memory/MemoryUtils.hh"

namespace lr::Memory
{
struct LinearAllocatorView
{
    void Init(u64 size);

    bool CanAllocate(u64 size, u32 alignment);
    u64 Allocate(u64 size, u32 alignment);
    void Grow(u64 size);
    void Free();

    u64 m_Offset = 0;
    u64 m_Size = 0;
};

struct LinearAllocatorViewAtomic
{
    void Init(u64 size);

    bool CanAllocate(u64 size, u32 alignment);
    u64 Allocate(u64 size, u32 alignment);
    void Grow(u64 size);
    void Free();

    eastl::atomic<u64> m_Offset = 0;
    eastl::atomic<u64> m_Size = 0;
};

struct LinearAllocatorDesc
{
    u64 m_DataSize = 0;
    u64 m_AutoGrowSize = 0;  // 0 disables autogrow
    void *m_pInitialData = nullptr;
};

template<typename _View>
struct LinearAllocatorBase
{
    void Init(const LinearAllocatorDesc &desc);
    void Delete();
    bool CanAllocate(u64 size, u32 alignment = 1);
    void *Allocate(u64 size, u32 alignment = 1);
    void Free(bool freeData);
    void Reserve(u64 size);

    _View m_View;
    u8 *m_pData = nullptr;
    bool m_SelfAllocated = false;
    u32 m_AutoGrowSize = 0;
};

using LinearAllocator = LinearAllocatorBase<LinearAllocatorView>;
using LinearAllocatorAtomic = LinearAllocatorBase<LinearAllocatorViewAtomic>;

//////////////////////////////////////////
/// LinearAllocatorBase Implementation ///
//////////////////////////////////////////

template<typename _View>
void LinearAllocatorBase<_View>::Init(const LinearAllocatorDesc &desc)
{
    ZoneScoped;

    m_View.Init(desc.m_DataSize);
    m_pData = (u8 *)desc.m_pInitialData;
    m_SelfAllocated = !(bool)m_pData;

    if (!m_pData)
        m_pData = Memory::Allocate<u8>(desc.m_DataSize);
}

template<typename _View>
void LinearAllocatorBase<_View>::Delete()
{
    ZoneScoped;

    if (m_SelfAllocated)
        Memory::Release(m_pData);
}

template<typename _View>
bool LinearAllocatorBase<_View>::CanAllocate(u64 size, u32 alignment)
{
    ZoneScoped;

    return m_View.CanAllocate(size, alignment);
}

template<typename _View>
void *LinearAllocatorBase<_View>::Allocate(u64 size, u32 alignment)
{
    ZoneScoped;

    u64 offset = m_View.Allocate(size, alignment);
    if (offset == -1)
    {
        if (m_AutoGrowSize != 0)
        {
            m_View.Grow(m_AutoGrowSize);
            Reserve(m_View.m_Size);
            offset = m_View.Allocate(size, alignment);
        }
        else
        {
            return nullptr;
        }
    }

    return m_pData + offset;
}

template<typename _View>
void LinearAllocatorBase<_View>::Free(bool freeData)
{
    ZoneScoped;

    m_View.Free();

    if (freeData)
        Memory::Release(m_pData);
}

template<typename _View>
void LinearAllocatorBase<_View>::Reserve(u64 size)
{
    ZoneScoped;

    Memory::Reallocate(m_pData, size);
}

}  // namespace lr::Memory