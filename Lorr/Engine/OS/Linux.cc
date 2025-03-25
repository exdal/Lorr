#include "Engine/OS/OS.hh"

#include <sys/file.h>
#include <sys/inotify.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/sysinfo.h>
#include <unistd.h>

#include <iostream>

typedef struct inotify_event inotify_event_t;

namespace lr {
// TODO: Properly handle all errno cases.
auto os::file_open(const fs::path &path, FileAccess access) -> std::expected<FileDescriptor, FileResult> {
    ZoneScoped;

    errno = 0;

    i32 flags = O_CREAT | O_WRONLY | O_RDONLY | O_TRUNC;
    if (access & FileAccess::Write)
        flags &= ~O_RDONLY;
    if (access & FileAccess::Read)
        flags &= ~(O_WRONLY | O_CREAT | O_TRUNC);

    i32 file = open64(path.c_str(), flags, S_IRUSR | S_IWUSR);
    if (file < 0) {
        switch (errno) {
            case EACCES:
                return std::unexpected(FileResult::NoAccess);
            case EEXIST:
                return std::unexpected(FileResult::Exists);
            case EISDIR:
                return std::unexpected(FileResult::IsDir);
            case EBUSY:
                return std::unexpected(FileResult::InUse);
            default:
                return std::unexpected(FileResult::Unknown);
        }
    }

    return static_cast<FileDescriptor>(file);
}

auto os::file_close(FileDescriptor file) -> void {
    ZoneScoped;

    close(static_cast<i32>(file));
}

auto os::file_size(FileDescriptor file) -> std::expected<usize, FileResult> {
    ZoneScoped;

    errno = 0;

    struct stat st = {};
    fstat(static_cast<i32>(file), &st);
    if (errno != 0) {
        return std::unexpected(FileResult::Unknown);
    }

    return st.st_size;
}

auto os::file_read(FileDescriptor file, void *data, usize size) -> usize {
    ZoneScoped;

    u64 read_bytes_size = 0;
    u64 target_size = size;
    while (read_bytes_size < target_size) {
        u64 remainder_size = target_size - read_bytes_size;
        u8 *cur_data = reinterpret_cast<u8 *>(data) + read_bytes_size;

        errno = 0;
        iptr cur_read_size = read(static_cast<i32>(file), cur_data, remainder_size);
        if (cur_read_size < 0_iptr) {
            LOG_TRACE("File read interrupted! {}", cur_read_size);
            break;
        }

        read_bytes_size += cur_read_size;
    }

    return read_bytes_size;
}

auto os::file_write(FileDescriptor file, const void *data, usize size) -> usize {
    ZoneScoped;

    u64 written_bytes_size = 0;
    u64 target_size = size;
    while (written_bytes_size < target_size) {
        u64 remainder_size = target_size - written_bytes_size;
        const u8 *cur_data = reinterpret_cast<const u8 *>(data) + written_bytes_size;

        errno = 0;
        iptr cur_written_size = write(static_cast<i32>(file), cur_data, remainder_size);
        if (cur_written_size < 0_iptr) {
            break;
        }

        written_bytes_size += cur_written_size;
    }

    return written_bytes_size;
}

auto os::file_seek(FileDescriptor file, i64 offset) -> void {
    ZoneScoped;

    lseek64(static_cast<i32>(file), offset, SEEK_SET);
}

auto os::file_dialog(std::string_view title, FileDialogFlag flags) -> ls::option<fs::path> {
    ZoneScoped;

    std::cout.flush();
    if (std::system("zenity --version") != 0) {
        LOG_ERROR("Zenity is not installed on the system.");
        return ls::nullopt;
    }

    auto cmd = fmt::format("zenity --file-selection --title=\"{}\" ", title);
    if (flags & FileDialogFlag::DirOnly) {
        cmd += fmt::format("--directory ");
    }
    if (flags & FileDialogFlag::Save) {
        cmd += fmt::format("--save ");
    }
    if (flags & FileDialogFlag::Multiselect) {
        cmd += fmt::format("--multiple ");
    }

    c8 pipe_data[2048] = {};
    auto *pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        return ls::nullopt;
    }

    fgets(pipe_data, ls::count_of(pipe_data) - 1, pipe);
    std::string path_str = pipe_data;
    if (path_str.back() == '\n') {
        path_str.pop_back();
    }

    return path_str;
}

void os::file_stdout(std::string_view str) {
    ZoneScoped;

    write(STDOUT_FILENO, str.data(), str.length());
}

void os::file_stderr(std::string_view str) {
    ZoneScoped;

    write(STDERR_FILENO, str.data(), str.length());
}

auto os::file_watcher_init(const fs::path &) -> std::expected<FileWatcherDescriptor, FileResult> {
    ZoneScoped;

    FileWatcherDescriptor result = {};
    result.handle = static_cast<FileDescriptor>(inotify_init1(IN_NONBLOCK | IN_CLOEXEC));

    return result;
}

auto os::file_watcher_destroy(FileWatcherDescriptor &watcher) -> void {
    ZoneScoped;

    os::file_close(watcher.handle);
}

auto os::file_watcher_add(FileWatcherDescriptor &watcher, const fs::path &path) -> std::expected<FileDescriptor, FileResult> {
    ZoneScoped;
    errno = 0;

    i32 descriptor = inotify_add_watch(static_cast<i32>(watcher.handle), path.c_str(), IN_MOVE | IN_CLOSE | IN_CREATE | IN_DELETE);
    if (descriptor < 0) {
        switch (errno) {
            case EACCES:
                return std::unexpected(FileResult::NoAccess);
            case EEXIST:
                return std::unexpected(FileResult::Exists);
            default:
                return std::unexpected(FileResult::Unknown);
        }
    }

    return static_cast<FileDescriptor>(descriptor);
}

auto os::file_watcher_remove(FileWatcherDescriptor &watcher, FileDescriptor watch_descriptor) -> void {
    ZoneScoped;

    inotify_rm_watch(static_cast<i32>(watcher.handle), static_cast<i32>(watch_descriptor));
}

auto os::file_watcher_read(FileWatcherDescriptor &watcher, u8 *buffer, usize buffer_size) -> std::expected<i64, FileResult> {
    ZoneScoped;

    errno = 0;
    auto file_sock = static_cast<i32>(watcher.handle);
    auto read_size = read(file_sock, buffer, buffer_size);
    if (read_size < 0) {
        switch (errno) {
            case EBADF: {
                return std::unexpected(FileResult::BadFileDescriptor);
            }
            case EISDIR: {
                return std::unexpected(FileResult::IsDir);
            }
            case EINTR: {
                return std::unexpected(FileResult::Interrupted);
            }
            default: {
                return std::unexpected(FileResult::Unknown);
            }
        }
    }

    return read_size;
}

auto os::file_watcher_peek(FileWatcherDescriptor &, u8 *buffer, i64 &buffer_offset) -> FileEvent {
    ZoneScoped;

    auto *event_data = reinterpret_cast<inotify_event_t *>(buffer + buffer_offset);
    buffer_offset += static_cast<i64>(sizeof(inotify_event_t)) + event_data->len;

    FileActionMask action_mask = FileActionMask::None;
    if (event_data->len && !(event_data->mask & IN_ISDIR)) {
        // File action
        constexpr static auto FILE_CREATE_MASK = IN_CREATE | IN_MOVED_TO;
        constexpr static auto FILE_DELETE_MASK = IN_DELETE | IN_MOVED_FROM;
        constexpr static auto FILE_MODIFY_MASK = IN_CLOSE_WRITE | IN_CLOSE_NOWRITE;

        if (event_data->mask & FILE_CREATE_MASK) {
            action_mask |= FileActionMask::Create;
        }

        if (event_data->mask & FILE_DELETE_MASK) {
            action_mask |= FileActionMask::Delete;
        }

        if (event_data->mask & FILE_MODIFY_MASK) {
            action_mask |= FileActionMask::Modify;
        }
    } else {
        // Directory action
        constexpr static auto DIR_CREATE_MASK = IN_CREATE | IN_MOVE_SELF | IN_MOVED_TO;
        constexpr static auto DIR_DELETE_MASK = IN_DELETE | IN_DELETE_SELF | IN_UNMOUNT | IN_MOVED_FROM;

        if (event_data->mask & DIR_CREATE_MASK) {
            action_mask |= FileActionMask::Directory | FileActionMask::Create;
        }

        if (event_data->mask & DIR_DELETE_MASK) {
            action_mask |= FileActionMask::Directory | FileActionMask::Delete;
        }
    }

    return FileEvent{
        .file_name = event_data->name,
        .action_mask = action_mask,
        .watch_descriptor = static_cast<FileDescriptor>(event_data->wd),
    };
}

auto os::file_watcher_buffer_size() -> i64 {
    return sizeof(inotify_event_t) + 1024 + 1;
}

auto os::mem_page_size() -> u64 {
    return sysconf(_SC_PAGESIZE);
}

auto os::mem_reserve(u64 size) -> void * {
    ZoneScoped;

    return mmap(nullptr, size, PROT_NONE, MAP_PRIVATE | MAP_ANON, -1, 0);
}

auto os::mem_release(void *data, u64 size) -> void {
    ZoneScoped;

    munmap(data, size);
}

auto os::mem_commit(void *data, u64 size) -> bool {
    ZoneScoped;

    return mprotect(data, size, PROT_READ | PROT_WRITE);
}

auto os::mem_decommit(void *data, u64 size) -> void {
    ZoneScoped;

    madvise(data, size, MADV_DONTNEED);
    mprotect(data, size, PROT_NONE);
}

auto os::thread_id() -> i64 {
    ZoneScoped;

    return syscall(__NR_gettid);
}

auto os::tsc() -> u64 {
    ZoneScoped;

#if defined(LS_COMPILER_CLANG) || defined(LS_COMPILER_GCC)
    return __builtin_ia32_rdtsc();
#else
    #error Unknown compiler on Linux
#endif
}

auto os::unix_timestamp() -> i64 {
    ZoneScoped;

    struct timespec ts = {};
    if (clock_gettime(CLOCK_REALTIME, &ts) == -1) {
        return 0;
    }

    return ts.tv_sec;
}

} // namespace lr
