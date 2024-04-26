#include "OS.hh"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

namespace lr {
u64 os::page_size()
{
    SYSTEM_INFO sys_info = {};
    GetSystemInfo(&sys_info);
    return sys_info.dwPageSize;
}

void *os::reserve(u64 size)
{
    return VirtualAlloc(nullptr, size, MEM_RESERVE, PAGE_READWRITE);
}

void os::release(void *data, u64 size)
{
    VirtualFree(data, size, MEM_RELEASE);
}

bool os::commit(void *data, u64 size)
{
    return VirtualAlloc(data, size, MEM_COMMIT, PAGE_READWRITE) != nullptr;
}

void os::decommit(void *data, u64 size)
{
    VirtualFree(data, size, MEM_DECOMMIT);
}
}  // namespace lr
