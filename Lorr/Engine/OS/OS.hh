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

enum class FileDialogFlag : u32 {
    None = 0,
    DirOnly = 1 << 0,
    Save = 1 << 1,
    Multiselect = 1 << 2,
};
consteval void enable_bitmask(FileDialogFlag);

// Can we add stdout and other pipes here?
enum class FileDescriptor : uptr { Invalid = 0 };

enum class FileActionMask : u32 {
    None = 0,
    Create = 1 << 0,
    Delete = 1 << 1,
    Modify = 1 << 2,
    Directory = 1 << 3,
};
consteval void enable_bitmask(FileActionMask);

struct FileEvent {
    std::string_view file_name = {};
    FileActionMask action_mask = FileActionMask::None;
    FileDescriptor watch_descriptor = FileDescriptor::Invalid;
};

namespace os {
    auto file_open(const fs::path &path, FileAccess access) -> std::expected<FileDescriptor, FileResult>;
    auto file_close(FileDescriptor file) -> void;
    auto file_size(FileDescriptor file) -> std::expected<usize, FileResult>;
    auto file_read(FileDescriptor file, void *data, usize size) -> usize;
    auto file_write(FileDescriptor file, const void *data, usize size) -> usize;
    auto file_seek(FileDescriptor file, i64 offset) -> void;
    auto file_dialog(std::string_view title, FileDialogFlag dialog_flags) -> ls::option<fs::path>;
    auto file_stdout(std::string_view str) -> void;
    auto file_stderr(std::string_view str) -> void;

    // https://man7.org/linux/man-pages/man7/inotify.7.html
    // https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-readdirectorychangesw
    //
    auto file_watcher_init() -> FileDescriptor;
    auto file_watcher_add(FileDescriptor watcher, const fs::path &path) -> std::expected<FileDescriptor, FileResult>;
    auto file_watcher_remove(FileDescriptor watcher, FileDescriptor watch_descriptor) -> void;
    auto file_watcher_read(FileDescriptor watcher, u8 *buffer, usize buffer_size) -> std::expected<i64, FileResult>;
    auto file_watcher_peek(u8 *buffer, i64 &buffer_offset) -> FileEvent;
    auto file_watcher_buffer_size() -> i64;

    //  ── MEMORY ──────────────────────────────────────────────────────────
    auto mem_page_size() -> u64;
    auto mem_reserve(u64 size) -> void *;
    auto mem_release(void *data, u64 size = 0) -> void;
    auto mem_commit(void *data, u64 size) -> bool;
    auto mem_decommit(void *data, u64 size) -> void;
}  // namespace os
}  // namespace lr
