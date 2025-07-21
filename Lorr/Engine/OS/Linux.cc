#include "Engine/OS/OS.hh"

#include "Engine/Memory/Stack.hh"

#include <sys/file.h>
#include <sys/inotify.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/sysinfo.h>
#include <unistd.h>

#include <pthread.h>

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

void os::file_stdout(std::string_view str) {
    ZoneScoped;

    write(STDOUT_FILENO, str.data(), str.length());
}

void os::file_stderr(std::string_view str) {
    ZoneScoped;

    write(STDERR_FILENO, str.data(), str.length());
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

auto os::set_thread_name(std::string_view name) -> void {
    ZoneScoped;
    memory::ScopedStack stack;

    pthread_setname_np(pthread_self(), stack.null_terminate_cstr(name));
}

auto os::set_thread_name(std::thread::native_handle_type thread, std::string_view name) -> void {
    ZoneScoped;
    memory::ScopedStack stack;

    pthread_setname_np(thread, stack.null_terminate_cstr(name));
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
