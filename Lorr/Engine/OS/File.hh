#pragma once

#include "Engine/OS/OS.hh"

namespace lr {
struct File {
    ls::option<FileDescriptor> handle;
    usize size = 0;
    FileResult result = FileResult::Success;

    File() = default;
    File(const fs::path &path, FileAccess access);
    File(const File &) = default;
    File(File &&) = default;
    ~File() {
        close();
    }

    auto write(const void *data, usize data_size) -> u64;
    auto read(void *data, usize data_size) -> u64;
    auto seek(i64 offset) -> void;
    auto close() -> void;

    static auto to_bytes(const fs::path &path) -> std::vector<u8>;
    static auto to_string(const fs::path &path) -> std::string;
    static auto to_stdout(std::string_view str) -> void;
    static auto to_stderr(std::string_view str) -> void;

    File &operator=(File &&rhs) noexcept {
        this->handle = rhs.handle;
        this->size = rhs.size;
        this->result = rhs.result;

        rhs.handle.reset();

        return *this;
    }
    bool operator==(const File &) const = default;
    explicit operator bool() {
        return result == FileResult::Success;
    }
};

} // namespace lr
