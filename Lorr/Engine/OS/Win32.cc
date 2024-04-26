#include "OS.hh"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

namespace lr {
/// FILE SYSTEM ///
Result<os::File, os::FileResult> os::open_file(std::string_view path, FileAccess access)
{
    ZoneScoped;
    return {};
}

void os::close_file(File &file)
{
    ZoneScoped;
}

Result<u64, os::FileResult> os::file_size(File file)
{
    ZoneScoped;
    return {};
}

// https://pubs.opengroup.org/onlinepubs/7908799/xsh/read.html
u64 os::read_file(File file, void *data, u64range range)
{
    ZoneScoped;
    return {};
}

void os::write_file(File file, const void *data, u64range range)
{
    ZoneScoped;
}

/// MEMORY ///
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
