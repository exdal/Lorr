//
// Created on Tuesday 2nd May 2023 by exdal
//

#pragma once

#include "Memory/Allocator/LinearAllocator.hh"

namespace lr::lt
{
/// "Growing Vector" is a view to it's bound allocator.
/// It cannot deallocate any data, however it can modify allocator offset
/// by calling `pop_back` or `clear`, etc...
template<typename _Type>
struct growing_vector
{
    growing_vector(Memory::LinearAllocator &allocator);
    growing_vector(Memory::LinearAllocator &allocator, u64 initialSize);
    ~growing_vector();

    void push_back(const _Type &var);
    void pop_back();

    u64 size();
    u64 size_bytes();

    const _Type *begin() const;
    _Type *begin();

    const _Type *end() const;
    _Type *end();

    _Type *data();

    _Type &operator[](u64 n);
    const _Type &operator[](u64 n) const;

    void reset();

    Memory::LinearAllocator &m_Allocator;
};

template<typename _Type>
inline growing_vector<_Type>::growing_vector(Memory::LinearAllocator &allocator)
    : m_Allocator(allocator)
{
}

template<typename _Type>
inline growing_vector<_Type>::growing_vector(Memory::LinearAllocator &allocator, u64 initialSize)
    : m_Allocator(allocator)
{
    if (initialSize > m_Allocator.m_View.m_Size)
        m_Allocator.Reserve(initialSize);
}

template<typename _Type>
inline growing_vector<_Type>::~growing_vector()
{
    reset();
}

template<typename _Type>
inline void growing_vector<_Type>::push_back(const _Type &var)
{
    ZoneScoped;

    Memory::AllocationInfo info = {
        .m_Size = sizeof(_Type),
        .m_Alignment = alignof(_Type),
    };

    void *pData = m_Allocator.Allocate(info);
    assert(pData && "Cannot allocate more data!");

    memcpy(pData, &var, sizeof(_Type));
}

template<typename _Type>
inline void growing_vector<_Type>::pop_back()
{
    ZoneScoped;

    u64 &offset = m_Allocator.m_View.m_Offset;
    offset = eastl::max<i64, i64>(0, offset - sizeof(_Type));
}

template<typename _Type>
inline u64 growing_vector<_Type>::size()
{
    ZoneScoped;

    return m_Allocator.m_View.m_Offset / sizeof(_Type);
}

template<typename _Type>
inline u64 growing_vector<_Type>::size_bytes()
{
    ZoneScoped;

    return m_Allocator.m_View.m_Offset;
}

template<typename _Type>
inline const _Type *growing_vector<_Type>::begin() const
{
    ZoneScoped;

    return m_Allocator.m_pData;
}

template<typename _Type>
inline _Type *growing_vector<_Type>::begin()
{
    ZoneScoped;

    return m_Allocator.m_pData;
}

template<typename _Type>
inline const _Type *growing_vector<_Type>::end() const
{
    ZoneScoped;

    return begin() + size();
}

template<typename _Type>
inline _Type *growing_vector<_Type>::end()
{
    ZoneScoped;

    return begin() + size();
}

template<typename _Type>
inline _Type *growing_vector<_Type>::data()
{
    ZoneScoped;

    return begin();
}

template<typename _Type>
inline _Type &growing_vector<_Type>::operator[](u64 n)
{
    ZoneScoped;

    return *(begin() + n);
}

template<typename _Type>
inline const _Type &growing_vector<_Type>::operator[](u64 n) const
{
    ZoneScoped;

    return *(begin() + n);
}

template<typename _Type>
inline void growing_vector<_Type>::reset()
{
    ZoneScoped;

    m_Allocator.m_View.m_Offset = 0;
}

}  // namespace lr::lt