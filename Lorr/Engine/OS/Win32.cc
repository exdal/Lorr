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

    // OPENFILENAME ofn = {};

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

constexpr auto WATCH_FILTERS =
    FILE_NOTIFY_CHANGE_CREATION | FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE;

auto os::file_watcher_init(const fs::path &root_dir) -> std::expected<FileWatcherDescriptor, FileResult> {
    ZoneScoped;

    FileWatcherDescriptor result = {};
    auto handle = CreateFile(
        root_dir.c_str(),
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
        nullptr
    );
    auto event_handle = CreateEvent(nullptr, false, false, nullptr);

    result.handle = static_cast<FileDescriptor>(reinterpret_cast<iptr>(handle));
    result.event = reinterpret_cast<iptr>(event_handle);

    return result;
}

auto os::file_watcher_destroy(FileWatcherDescriptor &watcher) -> void {
    ZoneScoped;

    os::file_close(watcher.handle);
    CloseHandle(reinterpret_cast<HANDLE>(watcher.event));
}

auto os::file_watcher_add(FileWatcherDescriptor &, const fs::path &) -> std::expected<FileDescriptor, FileResult> {
    ZoneScoped;

    thread_local usize unique_counter = 0;
    // Win32 doesn't support individual watches
    return static_cast<FileDescriptor>(++unique_counter);
}

auto os::file_watcher_remove(FileWatcherDescriptor &, FileDescriptor) -> void {
    ZoneScoped;
}

auto os::file_watcher_read(FileWatcherDescriptor &watcher, u8 *buffer, usize buffer_size) -> std::expected<i64, FileResult> {
    ZoneScoped;

    // This shit is pretty retarded. Fuck you microsoft
#if 0
    auto event_handle = reinterpret_cast<HANDLE>(watcher.event);
    auto handle = reinterpret_cast<HANDLE>(watcher.handle);
    OVERLAPPED overlapped = {};
    overlapped.hEvent = event_handle;

    ReadDirectoryChangesW(handle, buffer, buffer_size, true, WATCH_FILTERS, nullptr, &overlapped, nullptr);
    // TODO: Game loop too slow to handle this, we are losing data.
    if (WaitForSingleObject(event_handle, 0) == WAIT_OBJECT_0) {
        DWORD bytes_read = 0;
        GetOverlappedResult(handle, &overlapped, &bytes_read, false);
        return static_cast<i64>(bytes_read);
    }
#endif
    return 0;
}

auto os::file_watcher_peek(FileWatcherDescriptor &watcher, u8 *buffer, i64 &buffer_offset) -> FileEvent {
    ZoneScoped;

    auto *event_data = reinterpret_cast<FILE_NOTIFY_INFORMATION *>(buffer);
    buffer_offset += event_data->NextEntryOffset;

    FileActionMask action_mask = FileActionMask::None;
    auto file_name_str = std::wstring(event_data->FileName, event_data->FileNameLength / sizeof(wchar_t));
    auto file_name = fs::path(std::move(file_name_str));
    switch (event_data->Action) {
        case FILE_ACTION_ADDED: {
            action_mask |= FileActionMask::Create;
        } break;
        case FILE_ACTION_REMOVED: {
            action_mask |= FileActionMask::Delete;
        } break;
        case FILE_ACTION_MODIFIED: {
            action_mask |= FileActionMask::Modify;
        } break;
        default:
            break;
    }

    LOG_TRACE("FileName: {}", file_name);
    return FileEvent {
        .file_name = std::move(file_name),
        .action_mask = action_mask,
        .watch_descriptor = static_cast<FileDescriptor>(1),
    };
}

auto os::file_watcher_buffer_size() -> i64 {
    ZoneScoped;

    return sizeof(FILE_NOTIFY_INFORMATION) + 1024_i64;
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

auto os::mem_release(void *data, [[maybe_unused]] u64 size) -> void {
    ZoneScoped;
    TracyFree(data);
    VirtualFree(data, 0, MEM_RELEASE);
}

auto os::mem_commit(void *data, u64 size) -> bool {
    ZoneScoped;
    TracyAllocN(data, size, "Virtual Alloc");
    return VirtualAlloc(data, size, MEM_COMMIT, PAGE_READWRITE) != nullptr;
}

auto os::mem_decommit(void *data, [[maybe_unused]] u64 size) -> void {
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
    li_timestamp.HighPart = static_cast<LONG>(ft.dwHighDateTime);

    return (li_timestamp.QuadPart - UNIX_TIME_START) / TICKS_PER_SECOND;
}

} // namespace lr
