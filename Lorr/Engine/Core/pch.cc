#include "pch.hh"


#if TRACY_ENABLE
#define TRACY_ALLOC(ptr, size) TracySecureAlloc(ptr, size)
#define TRACY_FREE(ptr) TracySecureFree(ptr)
#else
#define TRACY_ALLOC(ptr, size) (void)0
#define TRACY_FREE(ptr) (void)0
#endif

/// EA ALLOCATOR
void *operator new(size_t size)
{
    void *PTR = _aligned_offset_malloc(size, 16, 0);
    TRACY_ALLOC(PTR, size);
    return PTR;
}

void *operator new[](size_t size)
{
    void *PTR = _aligned_offset_malloc(size, 16, 0);
    TRACY_ALLOC(PTR, size);
    return PTR;
}

void *operator new[](size_t size, const char * /*name*/, int /*flags*/, unsigned /*debugFlags*/, const char * /*file*/, int /*line*/)
{
    void *PTR = _aligned_offset_malloc(size, 16, 0);
    TRACY_ALLOC(PTR, size);
    return PTR;
}

void *operator new[](size_t size, size_t alignment, size_t /*alignmentOffset*/, const char * /*name*/, int /*flags*/, unsigned /*debugFlags*/,
                     const char * /*file*/, int /*line*/)
{
    void *PTR = _aligned_offset_malloc(size, alignment, 0);
    TRACY_ALLOC(PTR, size);
    return PTR;
}

void *operator new(size_t size, size_t alignment)
{
    void *PTR = _aligned_offset_malloc(size, alignment, 0);
    TRACY_ALLOC(PTR, size);
    return PTR;
}

void *operator new(size_t size, size_t alignment, const std::nothrow_t &) EA_THROW_SPEC_NEW_NONE()
{
    void *PTR = _aligned_offset_malloc(size, alignment, 0);
    TRACY_ALLOC(PTR, size);
    return PTR;
}

void *operator new[](size_t size, size_t alignment)
{
    void *PTR = _aligned_offset_malloc(size, alignment, 0);
    TRACY_ALLOC(PTR, size);
    return PTR;
}

void *operator new[](size_t size, size_t alignment, const std::nothrow_t &) EA_THROW_SPEC_NEW_NONE()
{
    void *PTR = _aligned_offset_malloc(size, alignment, 0);
    TRACY_ALLOC(PTR, size);
    return PTR;
}

void operator delete[](void *p) noexcept
{
    TRACY_FREE(p);
    _aligned_free(p);
}

void operator delete(void *p, std::size_t sz)EA_THROW_SPEC_DELETE_NONE()
{
    TRACY_FREE(p);
    _aligned_free(p);
}

void operator delete[](void *p, std::size_t sz) EA_THROW_SPEC_DELETE_NONE()
{
    TRACY_FREE(p);
    _aligned_free(p);
}

void operator delete(void *p)EA_THROW_SPEC_DELETE_NONE()
{
    TRACY_FREE(p);
    _aligned_free(p);
}