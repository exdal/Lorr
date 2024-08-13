#include "OS.hh"

#include <sys/file.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/sysinfo.h>
#include <unistd.h>

namespace lr {

/// FILE SYSTEM ///
File::File(const fs::path &path, FileAccess access) {
    ZoneScoped;

    errno = 0;

    i32 flags = O_CREAT | O_WRONLY | O_RDONLY;
    if (access & FileAccess::Write)
        flags &= ~O_RDONLY;
    if (access & FileAccess::Read)
        flags &= ~O_WRONLY;

    i32 file = open64(path.c_str(), flags, S_IRUSR | S_IWUSR);
    if (file < 0) {
        switch (errno) {
            case EACCES:
                this->result = FileResult::NoAccess;
                break;
            case EEXIST:
                this->result = FileResult::Exists;
                break;
            case EISDIR:
                this->result = FileResult::IsDir;
                break;
            case EBUSY:
                this->result = FileResult::InUse;
                break;
            default:
                this->result = FileResult::Unknown;
                break;
        }

        return;
    }

    struct stat st = {};
    fstat(static_cast<i32>(file), &st);
    this->size = st.st_size;
    this->handle = static_cast<uptr>(file);
}

auto _write(i32 handle, const void *data, u64 size) -> u64 {
    return write(handle, data, size);
}

void File::write(this File &self, const void *data, ls::u64range range) {
    ZoneScoped;

    LR_CHECK(self.handle.has_value(), "Trying to write invalid file");

    u64 offset = range.min;
    u64 written_bytes_size = 0;
    u64 target_size = range.length();
    while (written_bytes_size < target_size) {
        u64 remainder_size = target_size - written_bytes_size;
        const u8 *cur_data = reinterpret_cast<const u8 *>(data) + offset;
        iptr cur_written_size = _write(static_cast<i32>(self.handle.value()), cur_data, remainder_size);
        if (cur_written_size < 0) {
            LR_LOG_TRACE("File write interrupted! {}", cur_written_size);
            break;
        }

        offset += cur_written_size;
        written_bytes_size += cur_written_size;
    }
}

auto _read(i32 handle, void *data, u64 size) -> u64 {
    return read(handle, data, size);
}

u64 File::read(this File &self, void *data, ls::u64range range) {
    ZoneScoped;

    LR_CHECK(self.handle.has_value(), "Trying to read invalid file");

    // `read` can be interrupted, it can read *some* bytes,
    // so we need to make it loop and carefully read whole data
    range.clamp(self.size);
    u64 read_bytes_size = 0;
    u64 target_size = range.length();
    while (read_bytes_size < target_size) {
        u64 remainder_size = target_size - read_bytes_size;
        u8 *cur_data = reinterpret_cast<u8 *>(data) + read_bytes_size;
        iptr cur_read_size = _read(static_cast<i32>(self.handle.value()), cur_data, remainder_size);
        if (cur_read_size < 0) {
            LR_LOG_TRACE("File read interrupted! {}", cur_read_size);
            break;
        }

        read_bytes_size += cur_read_size;
    }

    return read_bytes_size;
}

void File::seek(this File &self, u64 offset) {
    ZoneScoped;

    lseek64(static_cast<i32>(self.handle.value()), static_cast<i64>(offset), SEEK_SET);
}

void _close(i32 file) {
    close(file);
}

void File::close(this File &self) {
    ZoneScoped;

    _close(static_cast<i32>(self.handle.value()));
    self.handle.reset();
}

/// MEMORY ///
u64 mem_page_size() {
    return 0;
}

void *mem_reserve(u64 size) {
    return mmap(nullptr, size, PROT_NONE, MAP_PRIVATE | MAP_ANON, -1, 0);
}

void mem_release(void *data, u64 size) {
    TracyFree(data);
    munmap(data, size);
}

bool mem_commit(void *data, u64 size) {
    TracyAllocN(data, size, "Virtual Alloc");
    return mprotect(data, size, PROT_READ | PROT_WRITE);
}

void mem_decommit(void *data, u64 size) {
    madvise(data, size, MADV_DONTNEED);
    mprotect(data, size, PROT_NONE);
}
}  // namespace lr
