#pragma once

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

enum class FileAccess {
    Write = 1 << 0,
    Read = 1 << 1,
};

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

        auto data = std::make_unique_for_overwrite<u8[]>(self.size);
        self.read(data.get(), { 0, ~0_sz });

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

    File &operator=(File &&) = default;
    bool operator==(const File &) const = default;
    explicit operator bool() { return result == FileResult::Success; }

    static ls::option<std::string> open_dialog();
    static bool save_dialog(const fs::path &path);
};

/// MEMORY ///
u64 mem_page_size();
void *mem_reserve(u64 size);
void mem_release(void *data, u64 size = 0);
bool mem_commit(void *data, u64 size);
void mem_decommit(void *data, u64 size);

template<>
struct has_bitmask<FileAccess> : ls::true_type {};
}  // namespace lr
