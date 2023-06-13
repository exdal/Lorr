// Created on Tuesday February 28th 2023 by exdal
// Last modified on Monday June 12th 2023 by exdal

#pragma once

#include "Memory/Allocator/TLSFAllocator.hh"

#include "Vulkan.hh"

namespace lr::Graphics
{
enum class ResourceAllocator : u32
{
    None = 0,         // Resource will have it's own memory heap, can be used for sub-allocations
    Descriptor,       // LINEAR - small sized pool with CPUW flag for per-frame descriptor data
    BufferLinear,     // LINEAR - medium sized pool for buffers
    BufferTLSF,       // TLSF - Large sized pool for large scene buffers
    BufferTLSF_Host,  // TLSF - medium sized pool for host visible buffers like UI
    BufferFrametime,  // LINEAR - Small pool for frametime buffers
    ImageTLSF,        // TLSF - Large sized pool for large images

    Count,  // Special flag, basically telling API to not allocate anything (ie. SC images)
};

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

    void *operator new(size_t size) { return APIAllocator::g_Handle.Allocate(size); }

    void operator delete(void *pData) { APIAllocator::g_Handle.Free(pData); }
};

}  // namespace lr::Graphics