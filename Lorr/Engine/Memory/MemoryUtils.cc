#include "MemoryUtils.hh"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <intrin.h>

namespace lr {
/// FIND LEAST SET ///
u32 memory::find_least_set32(u32 v)
{
    i32 b = v ? 32 - __builtin_clz(v) : 0;
    return b - 1;
}

u32 memory::find_least_set64(u64 v)
{
    i32 b = v ? 64 - __builtin_clzll(v) : 0;
    return b - 1;
}

/// FIND FIRST SET ///
u32 memory::find_first_set32(u32 v)
{
    return __builtin_ffs((i32)v) - 1;
}

u32 memory::find_first_set64(u64 v)
{
    return __builtin_ffsll((i64)v) - 1;
}

/// VIRTUALALLOC ///

u64 memory::page_size()
{
    SYSTEM_INFO sys_info = {};
    GetSystemInfo(&sys_info);
    return sys_info.dwPageSize;
}

void *memory::reserve(u64 size)
{
    return VirtualAlloc(nullptr, size, MEM_RESERVE, PAGE_READWRITE);
}

void memory::release(void *data, u64 size)
{
    VirtualFree(data, size, MEM_RELEASE);
}

bool memory::commit(void *data, u64 size)
{
    return VirtualAlloc(data, size, MEM_COMMIT, PAGE_READWRITE) != nullptr;
}

void memory::decommit(void *data, u64 size)
{
    VirtualFree(data, size, MEM_DECOMMIT);
}
}  // namespace lr
