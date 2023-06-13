// Created on Saturday May 7th 2022 by exdal
// Last modified on Monday June 12th 2023 by exdal

#include "pch.hh"

template<typename T, typename U>
T AlignUp(T size, U alignment)
{
    return (size + (alignment - 1)) & ~(alignment - 1);
}

/// EA ALLOCATOR
void *operator new(size_t size)
{
    void *PTR = _aligned_offset_malloc(size, 16, 0);
    TracyAllocS(PTR, AlignUp(size, 16), 20);
    return PTR;
}

void *operator new[](size_t size)
{
    void *PTR = _aligned_offset_malloc(size, 16, 0);
    TracyAllocS(PTR, AlignUp(size, 16), 20);
    return PTR;
}

void *operator new[](
    size_t size,
    const char * /*name*/,
    int /*flags*/,
    unsigned /*debugFlags*/,
    const char * /*file*/,
    int /*line*/)
{
    void *PTR = _aligned_offset_malloc(size, 16, 0);
    TracyAllocS(PTR, AlignUp(size, 16), 20);
    return PTR;
}

void *operator new[](
    size_t size,
    size_t alignment,
    size_t /*alignmentOffset*/,
    const char * /*name*/,
    int /*flags*/,
    unsigned /*debugFlags*/,
    const char * /*file*/,
    int /*line*/)
{
    void *PTR = _aligned_offset_malloc(size, alignment, 0);
    TracyAllocS(PTR, AlignUp(size, alignment), 20);
    return PTR;
}

void *operator new(size_t size, size_t alignment)
{
    void *PTR = _aligned_offset_malloc(size, alignment, 0);
    TracyAllocS(PTR, AlignUp(size, alignment), 20);
    return PTR;
}

void *operator new(size_t size, size_t alignment, const std::nothrow_t &) EA_THROW_SPEC_NEW_NONE()
{
    void *PTR = _aligned_offset_malloc(size, alignment, 0);
    TracyAllocS(PTR, AlignUp(size, alignment), 20);
    return PTR;
}

void *operator new[](size_t size, size_t alignment)
{
    void *PTR = _aligned_offset_malloc(size, alignment, 0);
    TracyAllocS(PTR, AlignUp(size, alignment), 20);
    return PTR;
}

void *operator new[](size_t size, size_t alignment, const std::nothrow_t &) EA_THROW_SPEC_NEW_NONE()
{
    void *PTR = _aligned_offset_malloc(size, alignment, 0);
    TracyAllocS(PTR, AlignUp(size, alignment), 20);
    return PTR;
}

void operator delete[](void *p) noexcept
{
    TracyFreeS(p, 20);
    _aligned_free(p);
}

void operator delete(void *p, std::size_t sz) EA_THROW_SPEC_DELETE_NONE()
{
    TracyFreeS(p, 20);
    _aligned_free(p);
}

void operator delete[](void *p, std::size_t sz) EA_THROW_SPEC_DELETE_NONE()
{
    TracyFreeS(p, 20);
    _aligned_free(p);
}

void operator delete(void *p) EA_THROW_SPEC_DELETE_NONE()
{
    TracyFreeS(p, 20);
    _aligned_free(p);
}