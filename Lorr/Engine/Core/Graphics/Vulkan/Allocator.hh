//
// Created on Tuesday 28th February 2023 by exdal
//

#pragma once

#include "Core/Memory/Allocator/TLSFAllocator.hh"

#include "Vulkan.hh"

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

template<VkObjectType _ObjType>
struct APIObject
{
    static constexpr VkObjectType kObjectType = _ObjType;

    void *operator new(size_t size)
    {
        return APIAllocator::g_Handle.Allocate(size);
    }

    void operator delete(void *pData)
    {
        APIAllocator::g_Handle.Free(pData);
    }
};

}  // namespace lr::Graphics