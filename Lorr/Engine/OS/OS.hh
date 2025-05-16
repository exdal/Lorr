#pragma once

namespace lr {
//  ── FILE ────────────────────────────────────────────────────────────
// Semi implementation of libcxx std::errc::*
//
enum class FileResult : i32 {
    Success = 0,
    NoAccess,
    Exists,
    IsDir,
    InUse,
    Interrupted,
    BadFileDescriptor,
    Unknown,
};
constexpr bool operator!(FileResult v) {
    return v != FileResult::Success;
}

enum class FileAccess : u32 {
    Write = 1 << 0,
    Read = 1 << 1,
};
consteval void enable_bitmask(FileAccess);

// Can we add stdout and other pipes here?
enum class FileDescriptor : uptr { Invalid = 0 };

struct FileWatcherDescriptor {
    FileDescriptor handle = FileDescriptor::Invalid;
    iptr event = 0;
};
enum class FileActionMask : u32 {
    None = 0,
    Create = 1 << 0,
    Delete = 1 << 1,
    Modify = 1 << 2,
    Directory = 1 << 3,
};
consteval void enable_bitmask(FileActionMask);

struct FileEvent {
    fs::path file_name = {};
    FileActionMask action_mask = FileActionMask::None;
    FileDescriptor watch_descriptor = FileDescriptor::Invalid;
};

namespace os {
    //  ── IO ──────────────────────────────────────────────────────────────
    auto file_open(const fs::path &path, FileAccess access) -> std::expected<FileDescriptor, FileResult>;
    auto file_close(FileDescriptor file) -> void;
    auto file_size(FileDescriptor file) -> std::expected<usize, FileResult>;
    auto file_read(FileDescriptor file, void *data, usize size) -> usize;
    auto file_write(FileDescriptor file, const void *data, usize size) -> usize;
    auto file_seek(FileDescriptor file, i64 offset) -> void;
    auto file_stdout(std::string_view str) -> void;
    auto file_stderr(std::string_view str) -> void;

    // https://man7.org/linux/man-pages/man7/inotify.7.html
    // https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-readdirectorychangesw
    //
    auto file_watcher_init(const fs::path &root_dir) -> std::expected<FileWatcherDescriptor, FileResult>;
    auto file_watcher_destroy(FileWatcherDescriptor &watcher) -> void;
    auto file_watcher_add(FileWatcherDescriptor &watcher, const fs::path &path) -> std::expected<FileDescriptor, FileResult>;
    auto file_watcher_remove(FileWatcherDescriptor &watcher, FileDescriptor watch_descriptor) -> void;
    auto file_watcher_read(FileWatcherDescriptor &watcher, u8 *buffer, usize buffer_size) -> std::expected<i64, FileResult>;
    auto file_watcher_peek(FileWatcherDescriptor &watcher, u8 *buffer, i64 &buffer_offset) -> FileEvent;
    auto file_watcher_buffer_size() -> i64;

    //  ── MEMORY ──────────────────────────────────────────────────────────
    auto mem_page_size() -> u64;
    auto mem_reserve(u64 size) -> void *;
    auto mem_release(void *data, u64 size = 0) -> void;
    auto mem_commit(void *data, u64 size) -> bool;
    auto mem_decommit(void *data, u64 size) -> void;

    //  ── THREADS ─────────────────────────────────────────────────────────
    auto thread_id() -> i64;
    auto set_thread_name(std::string_view name) -> void;
    auto set_thread_name(std::thread::native_handle_type thread, std::string_view name) -> void;

    //  ── CLOCK ───────────────────────────────────────────────────────────
    auto tsc() -> u64;
    auto unix_timestamp() -> i64;
} // namespace os
} // namespace lr
