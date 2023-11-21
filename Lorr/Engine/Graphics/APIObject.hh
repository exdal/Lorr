#pragma once

#include "Memory/Allocator/TLSFAllocator.hh"

namespace lr::Graphics
{
struct APIAllocator
{
    void *allocate_type(u64 size);
    void free_type(void *pBlockAddr);

    Memory::TLSFAllocatorView m_type_allocator;
    u8 *m_type_data = nullptr;

    static APIAllocator m_g_handle;
};

struct APIObject
{
    void *operator new(usize size) { return APIAllocator::m_g_handle.allocate_type(size); }
    void operator delete(void *pData) { APIAllocator::m_g_handle.free_type(pData); }
};

template<typename T>
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