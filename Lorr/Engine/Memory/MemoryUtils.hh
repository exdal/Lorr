#pragma once

namespace lr::memory {
template<typename T>
T align_up(T size, T alignment)
{
    return (size + (alignment - 1)) & ~(alignment - 1);
}

template<typename T>
T align_down(T size, T alignment)
{
    return size & ~(alignment - 1);
}

constexpr u32 kib_to_bytes(const u32 x)
{
    return x << 10;
}

constexpr u32 mib_to_bytes(const u32 x)
{
    return x << 20;
}

/// FIND LEAST SET ///
u32 find_least_set32(u32 v);
u32 find_least_set64(u64 v);

/// FIND FIRST SET ///
u32 find_first_set32(u32 v);
u32 find_first_set64(u64 v);

/// VIRTUALALLOC ///

u64 page_size();
void *reserve(u64 size);
void release(void *data, u64 size = 0);
bool commit(void *data, u64 size);
void decommit(void *data, u64 size);

}  // namespace lr::memory
