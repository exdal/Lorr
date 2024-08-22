#pragma once

#include "Engine/OS/OS.hh"

namespace lr {
struct VirtualFile {
    std::unique_ptr<u8[]> contents = {};
    usize content_size = 0;

    const u8 *data() const { return contents.get(); }
    usize size() const { return content_size; }
};

struct VirtualFileInfo {
    std::string_view path = {};
    ls::span<const u8> data = {};
};

struct VirtualDir {
    ankerl::unordered_dense::map<std::string, VirtualFile> files = {};

    ls::option<std::reference_wrapper<VirtualFile>> add_file(const VirtualFileInfo &info) {
        auto data = std::make_unique_for_overwrite<u8[]>(info.data.size());
        std::memcpy(data.get(), info.data.data(), info.data.size());
        auto it = files.emplace(std::string(info.path), VirtualFile{ std::move(data), info.data.size() });
        return it.first->second;
    }

    ls::option<std::reference_wrapper<VirtualFile>> read_file(const std::string &virtual_path, const fs::path &real_path) {
        File file(real_path, FileAccess::Read);
        if (!file) {
            // john loguru hates wstring
            std::string str = real_path.string();
            LR_LOG_ERROR("Failed to read file '{}'!", str);
            return ls::nullopt;
        }

        auto it = files.emplace(virtual_path, VirtualFile{ file.whole_data(), file.size });
        return it.first->second;
    }
};

}  // namespace lr
