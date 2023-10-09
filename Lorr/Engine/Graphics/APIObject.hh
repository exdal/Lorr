// Created on Tuesday February 28th 2023 by exdal
// Last modified on Monday August 28th 2023 by exdal

#pragma once

#include <EASTL/type_traits.h>

#include "Memory/Allocator/TLSFAllocator.hh"

namespace lr::Graphics
{
struct APIAllocator
{
    void *Allocate(u64 size);
    void Free(void *pBlockAddr);

    Memory::TLSFAllocatorView m_TypeAllocator;
    u8 *m_pTypeData = nullptr;

    static APIAllocator g_Handle;
};

struct APIObject
{
    void *operator new(size_t size) { return APIAllocator::g_Handle.Allocate(size); }
    void operator delete(void *pData) { APIAllocator::g_Handle.Free(pData); }
};

template<typename _T>
struct ToVKObjectType
{
};

#define LR_ASSIGN_OBJECT_TYPE(_type, _val)                \
    template<>                                            \
    struct ToVKObjectType<_type>                          \
    {                                                     \
        constexpr static u32 type = _val;                 \
    };

}  // namespace lr::Graphics