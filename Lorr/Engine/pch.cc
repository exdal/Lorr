#ifdef TRACY_ENABLE

static void *lr_aligned_alloc(usize size, usize alignment = alignof(usize))
{
#if LR_WIN32 == 1
    return _aligned_malloc(size, alignment);
#elif LR_LINUX == 1
    void *data = nullptr;
    posix_memalign(&data, alignment, size);
    return data;
#else
#error "Unknown platform"
#endif
}

// https://en.cppreference.com/w/cpp/memory/new/operator_new
// Ignore non-allocating operators (for std::construct_at, placement new)

[[nodiscard]] void *operator new(std::size_t size)
{
    auto ptr = lr_aligned_alloc(size);
    TracyAlloc(ptr, size);
    return ptr;
}

void operator delete(void *ptr) noexcept
{
    TracyFree(ptr);
    free(ptr);
}

[[nodiscard]] void *operator new[](std::size_t size)
{
    auto ptr = lr_aligned_alloc(size);
    TracyAlloc(ptr, size);
    return ptr;
}

void operator delete[](void *ptr) noexcept
{
    TracyFree(ptr);
    free(ptr);
}

[[nodiscard]] void *operator new(std::size_t size, std::align_val_t alignment)
{
    auto ptr = lr_aligned_alloc(size, static_cast<usize>(alignment));
    TracyAlloc(ptr, size);
    return ptr;
}

void operator delete(void *ptr, std::align_val_t) noexcept
{
    TracyFree(ptr);
    free(ptr);
}

[[nodiscard]] void *operator new[](std::size_t size, std::align_val_t alignment)
{
    auto ptr = lr_aligned_alloc(size, static_cast<usize>(alignment));
    TracyAlloc(ptr, size);
    return ptr;
}

void operator delete[](void *ptr, std::align_val_t) noexcept
{
    TracyFree(ptr);
    free(ptr);
}

[[nodiscard]] void *operator new(std::size_t size, const std::nothrow_t &) noexcept
{
    auto ptr = lr_aligned_alloc(size);
    TracyAlloc(ptr, size);
    return ptr;
}

void operator delete(void *ptr, const std::nothrow_t &) noexcept
{
    TracyFree(ptr);
    free(ptr);
}

[[nodiscard]] void *operator new[](std::size_t size, const std::nothrow_t &) noexcept
{
    auto ptr = lr_aligned_alloc(size);
    TracyAlloc(ptr, size);
    return ptr;
}

void operator delete[](void *ptr, const std::nothrow_t &) noexcept
{
    TracyFree(ptr);
    free(ptr);
}

[[nodiscard]] void *operator new(std::size_t size, std::align_val_t alignment, const std::nothrow_t &) noexcept
{
    auto ptr = lr_aligned_alloc(size, static_cast<usize>(alignment));
    TracyAlloc(ptr, size);
    return ptr;
}

void operator delete(void *ptr, std::align_val_t, const std::nothrow_t &) noexcept
{
    TracyFree(ptr);
    free(ptr);
}

[[nodiscard]] void *operator new[](std::size_t size, std::align_val_t alignment, const std::nothrow_t &) noexcept
{
    auto ptr = lr_aligned_alloc(size, static_cast<usize>(alignment));
    TracyAlloc(ptr, size);
    return ptr;
}

void operator delete[](void *ptr, std::align_val_t, const std::nothrow_t &) noexcept
{
    TracyFree(ptr);
    free(ptr);
}

#endif
