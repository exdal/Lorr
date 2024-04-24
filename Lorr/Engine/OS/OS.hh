#pragma once

namespace lr::os {
/// FILE SYSTEM ///
LR_HANDLE(File, iptr);

enum class FileAccess {
    Write = 1 << 0,
    Read = 1 << 1,
};
LR_TYPEOP_ARITHMETIC(FileAccess, FileAccess, |);
LR_TYPEOP_ARITHMETIC_INT(FileAccess, FileAccess, &);

// These are not complete, some OS specific results may occur
enum class FileResult : i32 {
    Success = 0,
    NoAccess,
    Exists,
    IsDir,
    InUse,
};
constexpr bool operator!(FileResult v)
{
    return v == FileResult::Success;
}

Result<File, FileResult> open_file(std::string_view path, FileAccess access);
void close_file(File &file);
Result<u64, FileResult> file_size(File file);
u64 read_file(File file, void *data, u64range range);
void write_file(File file, const void *data, u64range range);

/// MEMORY ///
u64 page_size();
void *reserve(u64 size);
void release(void *data, u64 size = 0);
bool commit(void *data, u64 size);
void decommit(void *data, u64 size);
}  // namespace lr::os
