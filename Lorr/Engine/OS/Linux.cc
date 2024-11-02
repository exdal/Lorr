#include "OS.hh"

#include <sys/file.h>
#include <sys/inotify.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/sysinfo.h>
#include <unistd.h>

#include <iostream>

// TODO:
// Properly handle all errno cases.

namespace lr {
using std::to_underlying;

/// FILE SYSTEM ///
File::File(const fs::path &path, FileAccess access) {
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

    LS_EXPECT(self.handle.has_value());

    u64 data_offset = range.min;
    u64 written_bytes_size = 0;
    u64 target_size = range.length();
    while (written_bytes_size < target_size) {
        u64 remainder_size = target_size - written_bytes_size;
        const u8 *cur_data = reinterpret_cast<const u8 *>(data) + data_offset + written_bytes_size;

        errno = 0;
        iptr cur_written_size = _write(static_cast<i32>(self.handle.value()), cur_data, remainder_size);
        LS_EXPECT(errno == 0);

        if (cur_written_size < 0) {
            LOG_TRACE("File write interrupted! {}", cur_written_size);
            break;
        }

        written_bytes_size += cur_written_size;
    }
}

auto _read(i32 handle, void *data, u64 size) -> u64 {
    return read(handle, data, size);
}

u64 File::read(this File &self, void *data, ls::u64range range) {
    ZoneScoped;

    LS_EXPECT(self.handle.has_value());

    // `read` can be interrupted, it can read *some* bytes,
    // so we need to make it loop and carefully read whole data
    range.clamp(self.size);
    u64 read_bytes_size = 0;
    u64 target_size = range.length();
    while (read_bytes_size < target_size) {
        u64 remainder_size = target_size - read_bytes_size;
        u8 *cur_data = reinterpret_cast<u8 *>(data) + range.min + read_bytes_size;

        errno = 0;
        iptr cur_read_size = _read(static_cast<i32>(self.handle.value()), cur_data, remainder_size);
        if (cur_read_size < 0) {
            LOG_TRACE("File read interrupted! {}", cur_read_size);
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

    if (self.handle) {
        _close(static_cast<i32>(self.handle.value()));
        self.handle.reset();
    }
}

ls::option<fs::path> File::open_dialog(std::string_view title, FileDialogFlag flags) {
    ZoneScoped;

    std::cout.flush();
    if (std::system("zenity --version") != 0) {
        LOG_ERROR("Zenity is not installed on the system.");
        return ls::nullopt;
    }

    auto cmd = std::format("zenity --file-selection --title=\"{}\" ", title);
    if (flags & FileDialogFlag::DirOnly) {
        cmd += std::format("--directory ");
    }
    if (flags & FileDialogFlag::Save) {
        cmd += std::format("--save ");
    }
    if (flags & FileDialogFlag::Multiselect) {
        cmd += std::format("--multiple ");
    }

    c8 pipe_data[2048] = {};
    auto *pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        return ls::nullopt;
    }

    fgets(pipe_data, count_of(pipe_data) - 1, pipe);
    std::string path_str = pipe_data;
    if (path_str.back() == '\n') {
        path_str.pop_back();
    }

    return std::move(path_str);
}

void File::to_stdout(std::string_view str) {
    _write(STDOUT_FILENO, str.data(), str.length());
}

/// FILE ///
auto os_file_watcher_init() -> FileDescriptor {
    return static_cast<FileDescriptor>(inotify_init());
}

auto os_file_watcher_add(FileDescriptor watcher, const fs::path &path) -> std::expected<FileDescriptor, FileResult> {
    ZoneScoped;
    errno = 0;

    i32 descriptor = inotify_add_watch(static_cast<i32>(watcher), path.c_str(), IN_MOVE | IN_CLOSE | IN_CREATE | IN_DELETE);
    if (descriptor < 0) {
        switch (errno) {
            case EACCES:
                return std::unexpected(FileResult::NoAccess);
            case EEXIST:
                return std::unexpected(FileResult::Exists);
            default:
                return std::unexpected(static_cast<FileResult>(errno));
        }
    }

    return static_cast<FileDescriptor>(descriptor);
}

// holy fuck its cancer
auto os_file_watcher_read(FileDescriptor watcher, FileDescriptor socket) -> std::string {
    ZoneScoped;

    // constexpr usize buffer_size = sizeof(struct inotify_event) + 1024 + 1;
    // c8 buffer[buffer_size] = {};
    //
    // auto fd = static_cast<i32>(socket);
    // i32 read_bytes = select();

    return {};
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
