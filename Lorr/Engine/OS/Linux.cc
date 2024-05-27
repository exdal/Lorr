#include "OS.hh"

#include <sys/file.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/sysinfo.h>
#include <unistd.h>

namespace lr {

/// FILE SYSTEM ///
Result<os::File, os::FileResult> os::open_file(std::string_view path, FileAccess access)
{
    ZoneScoped;

    i32 flags = O_CREAT | O_WRONLY | O_RDONLY;
    if (access & FileAccess::Write)
        flags &= ~O_RDONLY;
    if (access & FileAccess::Read)
        flags &= ~O_WRONLY;

    i32 file = open64(path.data(), flags, S_IRUSR | S_IWUSR);
    if (file < 0) {
        return static_cast<FileResult>(file);
    }

    return static_cast<File>(file);
}

void os::close_file(File &file)
{
    ZoneScoped;

    close(static_cast<i32>(file));
    file = File::Invalid;
}

Result<u64, os::FileResult> os::file_size(File file)
{
    ZoneScoped;

    struct stat st = {};
    i32 result = fstat(static_cast<i32>(file), &st);
    if (result < 0) {
        LR_LOG_ERROR("Failed to open file! {}", result);
        return static_cast<FileResult>(result);
    }

    return st.st_size;
}

// https://pubs.opengroup.org/onlinepubs/7908799/xsh/read.html
u64 os::read_file(File file, void *data, u64range range)
{
    ZoneScoped;

    LR_CHECK(file != File::Invalid, "Trying to read invalid file");

    auto [max_read_size, _] = file_size(file);

    // `read` can be interrupted, it can read *some* bytes,
    // so we need to make it loop and carefully read whole data
    range.clamp(max_read_size);
    u64 offset = range.min;
    u64 read_bytes_size = 0;
    u64 target_size = range.length();
    while (read_bytes_size < target_size) {
        u64 remainder_size = target_size - read_bytes_size;
        u8 *cur_data = reinterpret_cast<u8 *>(data) + read_bytes_size;
        iptr cur_read_size = read(static_cast<i32>(file), cur_data, remainder_size);
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

    u64 offset = range.min;
    u64 written_bytes_size = 0;
    u64 target_size = range.length();
    while (written_bytes_size < target_size) {
        u64 remainder_size = target_size - written_bytes_size;
        const u8 *cur_data = reinterpret_cast<const u8 *>(data) + offset;
        iptr cur_written_size = write(static_cast<i32>(file), cur_data, remainder_size);
        if (cur_written_size < 0) {
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
    return 0;
}

void *os::reserve(u64 size)
{
    return mmap(nullptr, size, PROT_NONE, MAP_PRIVATE | MAP_ANON, -1, 0);
}

void os::release(void *data, u64 size)
{
    TracyFree(data);
    munmap(data, size);
}

bool os::commit(void *data, u64 size)
{
    TracyAllocN(data, size, "Virtual Alloc");
    return mprotect(data, size, PROT_READ | PROT_WRITE);
}

void os::decommit(void *data, u64 size)
{
    madvise(data, size, MADV_DONTNEED);
    mprotect(data, size, PROT_NONE);
}
}  // namespace lr
