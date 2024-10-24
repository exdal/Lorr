#pragma once

#include "Engine/Core/Handle.hh"

namespace lr {
// These are not complete, some OS specific results may occur
enum class FileResult : i32 {
    Success = 0,
    NoAccess,
    Exists,
    IsDir,
    InUse,
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

struct File {
    ls::option<uptr> handle;
    usize size = 0;
    FileResult result = FileResult::Success;

    File() = default;
    File(const fs::path &path, FileAccess access);
    File(const File &) = default;
    File(File &&) = default;
    ~File() { close(); }

    void write(this File &, const void *data, ls::u64range range);
    u64 read(this File &, void *data, ls::u64range range);
    void seek(this File &, u64 offset);

    void close(this File &);

    // Util functions
    std::unique_ptr<u8[]> whole_data(this File &self) {
        ZoneScoped;

        auto data = std::make_unique_for_overwrite<u8[]>(self.size + 1);
        self.read(data.get(), { 0, ~0_sz });
        data[self.size] = '\0';

        return data;
    }

    std::string read_string(this File &self, ls::u64range range) {
        ZoneScoped;

        range.clamp(self.size);
        std::string str;
        str.resize(range.length());
        self.read(str.data(), range);

        return str;
    }

    File &operator=(File &&rhs) noexcept {
        this->handle = rhs.handle;
        this->size = rhs.size;
        this->result = rhs.result;

        rhs.handle.reset();

        return *this;
    }
    bool operator==(const File &) const = default;
    explicit operator bool() { return result == FileResult::Success; }

    static ls::option<fs::path> open_dialog(std::string_view title, FileDialogFlag flags = FileDialogFlag::None);
    static void to_stdout(std::string_view str);
};

/// MEMORY ///
u64 mem_page_size();
void *mem_reserve(u64 size);
void mem_release(void *data, u64 size = 0);
bool mem_commit(void *data, u64 size);
void mem_decommit(void *data, u64 size);

}  // namespace lr
