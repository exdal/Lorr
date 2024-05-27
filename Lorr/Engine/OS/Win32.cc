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

    DWORD flags = 0;
    DWORD creation_flags = OPEN_EXISTING;
    DWORD share_flags = 0;
    if (access & FileAccess::Read) {
        flags |= GENERIC_READ;
        share_flags |= FILE_SHARE_READ;
    }
    if (access & FileAccess::Write) {
        flags |= GENERIC_WRITE;
        creation_flags |= CREATE_ALWAYS;
        share_flags |= FILE_SHARE_WRITE;
    }

    HANDLE file_handle = CreateFileA(path.data(), flags, share_flags, 0, creation_flags, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file_handle == INVALID_HANDLE_VALUE) {
        return FileResult::NoAccess;
    }

    return *reinterpret_cast<File *>(file_handle);
}

void os::close_file(File &file)
{
    ZoneScoped;

    HANDLE file_handle = reinterpret_cast<HANDLE>(file);
    CloseHandle(file_handle);
    file = File::Invalid;
}

Result<u64, os::FileResult> os::file_size(File file)
{
    ZoneScoped;

    usize size;
    HANDLE file_handle = reinterpret_cast<HANDLE>(file);
    GetFileSizeEx(file_handle, reinterpret_cast<LARGE_INTEGER *>(&size));

    return size;
}

u64 os::read_file(File file, void *data, u64range range)
{
    ZoneScoped;

    LR_CHECK(file != File::Invalid, "Trying to read invalid file");

    auto [max_read_size, _] = file_size(file);

    HANDLE file_handle = reinterpret_cast<HANDLE>(file);
    range.clamp(max_read_size);
    u64 offset = range.min;
    u64 read_bytes_size = 0;
    u64 target_size = range.length();
    while (read_bytes_size < target_size) {
        u64 remainder_size = target_size - read_bytes_size;
        u8 *cur_data = reinterpret_cast<u8 *>(data) + read_bytes_size;
        DWORD cur_read_size = 0;
        OVERLAPPED overlapped = {};
        overlapped.Offset = offset & 0x00000000ffffffffull;
        overlapped.OffsetHigh = (offset & 0xffffffff00000000ull) >> 32;
        ReadFile(file_handle, cur_data, remainder_size, &cur_read_size, &overlapped);
        if (cur_read_size < 0) {
            LR_LOG_TRACE("File read interrupted! {}", cur_read_size);
            break;
        }

        offset += cur_read_size;
        read_bytes_size += cur_read_size;
    }

    return read_bytes_size;
}

void os::write_file(File file, const void *data, u64range range)
{
    ZoneScoped;

    LR_CHECK(file != File::Invalid, "Trying to write invalid file");

    HANDLE file_handle = reinterpret_cast<HANDLE>(file);
    u64 offset = range.min;
    u64 written_bytes_size = 0;
    u64 target_size = range.length();
    while (written_bytes_size < target_size) {
        u64 remainder_size = target_size - written_bytes_size;
        const u8 *cur_data = reinterpret_cast<const u8 *>(data) + offset;
        DWORD cur_written_size = 0;
        OVERLAPPED overlapped = {};
        overlapped.Offset = offset & 0x00000000ffffffffull;
        overlapped.OffsetHigh = (offset & 0xffffffff00000000ull) >> 32;
        if (WriteFile(file_handle, cur_data, remainder_size, &cur_written_size, &overlapped) == 0) {
            LR_LOG_TRACE("File write interrupted! {}", cur_written_size);
            break;
        }

        offset += cur_written_size;
        written_bytes_size += cur_written_size;
    }
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
    TracyFree(data);
    VirtualFree(data, size, MEM_RELEASE);
}

bool os::commit(void *data, u64 size)
{
    TracyAllocN(data, size, "Virtual Alloc");
    return VirtualAlloc(data, size, MEM_COMMIT, PAGE_READWRITE) != nullptr;
}

void os::decommit(void *data, u64 size)
{
    VirtualFree(data, size, MEM_DECOMMIT);
}
}  // namespace lr
