#include "Engine/OS/OS.hh"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

namespace lr {
/// FILE SYSTEM ///
auto os::file_open(const fs::path &path, FileAccess access) -> std::expected<FileDescriptor, FileResult> {
    ZoneScoped;

    SetLastError(0);

    DWORD flags = 0;
    DWORD creation_flags = 0;
    DWORD share_flags = 0;
    if (access & FileAccess::Read) {
        flags |= GENERIC_READ;
        creation_flags |= OPEN_EXISTING;
        share_flags |= FILE_SHARE_READ;
    }
    if (access & FileAccess::Write) {
        flags |= GENERIC_WRITE;
        creation_flags |= CREATE_ALWAYS;
        share_flags |= FILE_SHARE_WRITE;
    }

    HANDLE file_handle = CreateFileW(path.c_str(), flags, share_flags, nullptr, creation_flags, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file_handle == INVALID_HANDLE_VALUE) {
        LOG_ERROR("Failed to open file! {}", GetLastError());
        return std::unexpected(FileResult::NoAccess);
    }

    return static_cast<FileDescriptor>(reinterpret_cast<iptr>(file_handle));
}

auto os::file_close(FileDescriptor file) -> void {
    ZoneScoped;

    HANDLE file_handle = reinterpret_cast<HANDLE>(file);
    CloseHandle(file_handle);
}

auto os::file_size(FileDescriptor file) -> std::expected<usize, FileResult> {
    ZoneScoped;

    LARGE_INTEGER li = {};
    GetFileSizeEx(reinterpret_cast<HANDLE>(file), &li);

    return static_cast<usize>(li.QuadPart);
}

auto os::file_read(FileDescriptor file, void *data, usize size) -> usize {
    ZoneScoped;

    auto file_handle = reinterpret_cast<HANDLE>(file);

    u64 read_bytes_size = 0;
    u64 target_size = size;
    while (read_bytes_size < target_size) {
        u64 remainder_size = target_size - read_bytes_size;
        u8 *cur_data = reinterpret_cast<u8 *>(data) + read_bytes_size;

        DWORD cur_read_size = 0;
        OVERLAPPED overlapped = {};
        overlapped.Offset = read_bytes_size & 0x00000000ffffffffull;
        overlapped.OffsetHigh = (read_bytes_size & 0xffffffff00000000ull) >> 32u;
        ReadFile(file_handle, cur_data, remainder_size, &cur_read_size, &overlapped);
        if (cur_read_size < 0) {
            LOG_TRACE("File read interrupted! {}", cur_read_size);
            break;
        }

        read_bytes_size += cur_read_size;
    }

    return read_bytes_size;
}

auto os::file_write(FileDescriptor file, const void *data, usize size) -> usize {
    ZoneScoped;

    HANDLE file_handle = reinterpret_cast<HANDLE>(file);
    u64 written_bytes_size = 0;
    u64 target_size = size;
    while (written_bytes_size < target_size) {
        u64 remainder_size = target_size - written_bytes_size;
        const u8 *cur_data = reinterpret_cast<const u8 *>(data) + written_bytes_size;
        DWORD cur_written_size = 0;
        OVERLAPPED overlapped = {};
        overlapped.Offset = written_bytes_size & 0x00000000ffffffffull;
        overlapped.OffsetHigh = (written_bytes_size & 0xffffffff00000000ull) >> 32;
        if (WriteFile(file_handle, cur_data, remainder_size, &cur_written_size, &overlapped) == 0) {
            LOG_TRACE("File write interrupted! {}", cur_written_size);
            break;
        }

        written_bytes_size += cur_written_size;
    }

    return written_bytes_size;
}

auto os::file_seek(FileDescriptor file, i64 offset) -> void {
    ZoneScoped;

    LARGE_INTEGER li = {};
    li.QuadPart = offset;
    SetFilePointerEx(reinterpret_cast<HANDLE>(file), li, nullptr, FILE_BEGIN);
}

auto os::file_dialog(std::string_view title, FileDialogFlag flags) -> ls::option<fs::path> {
    ZoneScoped;

    //OPENFILENAME ofn = {};

    return {};
}

auto os::file_stdout(std::string_view str) -> void {
    ZoneScoped;

    static auto stdout_hnd = GetStdHandle(STD_OUTPUT_HANDLE);

    DWORD written = 0;
    WriteFile(stdout_hnd, str.data(), str.length(), &written, nullptr);
}

auto os::file_stderr(std::string_view str) -> void {
    ZoneScoped;

    static auto stdout_hnd = GetStdHandle(STD_ERROR_HANDLE);

    DWORD written = 0;
    WriteFile(stdout_hnd, str.data(), str.length(), &written, nullptr);
}

auto os::file_watcher_init() -> FileDescriptor {
    ZoneScoped;

    return FileDescriptor::Invalid;
}

auto os::file_watcher_add(FileDescriptor watcher, const fs::path &path) -> std::expected<FileDescriptor, FileResult> {
    ZoneScoped;

    return FileDescriptor::Invalid;
}

auto os::file_watcher_remove(FileDescriptor watcher, FileDescriptor watch_descriptor) -> void {
    ZoneScoped;
}

auto os::file_watcher_read(FileDescriptor watcher, u8 *buffer, usize buffer_size) -> std::expected<i64, FileResult> {
    ZoneScoped;

    return {};
}

auto os::file_watcher_peek(u8 *buffer, i64 &buffer_offset) -> FileEvent {
    ZoneScoped;

    return {};
}

auto os::file_watcher_buffer_size() -> i64 {
    ZoneScoped;

    return {};
}

auto os::mem_page_size() -> u64 {
    ZoneScoped;

    SYSTEM_INFO sys_info = {};
    GetSystemInfo(&sys_info);
    return sys_info.dwPageSize;
}

auto os::mem_reserve(u64 size) -> void * {
    ZoneScoped;

    return VirtualAlloc(nullptr, size, MEM_RESERVE, PAGE_READWRITE);
}

auto os::mem_release(void *data, u64 size) -> void {
    ZoneScoped;
    TracyFree(data);
    VirtualFree(data, 0, MEM_RELEASE);
}

auto os::mem_commit(void *data, u64 size) -> bool {
    ZoneScoped;
    TracyAllocN(data, size, "Virtual Alloc");
    return VirtualAlloc(data, size, MEM_COMMIT, PAGE_READWRITE) != nullptr;
}

auto os::mem_decommit(void *data, u64 size) -> void {
    ZoneScoped;

    VirtualFree(data, 0, MEM_DECOMMIT | MEM_RELEASE);
}

auto os::thread_id() -> i64 {
    ZoneScoped;

    return GetCurrentThreadId();
}

auto os::tsc() -> u64 {
    ZoneScoped;

#if defined(LS_COMPILER_CLANG)
    return __builtin_ia32_rdtsc();
#else
    return static_cast<u64>(__rdtsc());
#endif
}

auto os::unix_timestamp() -> i64 {
    ZoneScoped;

    // https://stackoverflow.com/a/46024468
    constexpr auto UNIX_TIME_START = 0x019DB1DED53E8000_i64;
    constexpr auto TICKS_PER_SECOND = 10000000_i64;

    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);

    LARGE_INTEGER li_timestamp = {};
    li_timestamp.LowPart = ft.dwLowDateTime;
    li_timestamp.HighPart = ft.dwHighDateTime;

    return (li_timestamp.QuadPart - UNIX_TIME_START) / TICKS_PER_SECOND;
}

}  // namespace lr
