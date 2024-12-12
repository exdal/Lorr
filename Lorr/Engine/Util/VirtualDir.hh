#pragma once

#include "Engine/OS/File.hh"

namespace lr {
struct VirtualFile {
    std::vector<u8> contents = {};

    const u8 *data() const { return contents.data(); }
    usize size() const { return contents.size(); }
};

struct VirtualFileInfo {
    std::string_view path = {};
    ls::span<const u8> data = {};
};

struct VirtualDir {
    ankerl::unordered_dense::map<std::string, VirtualFile> files = {};

    ls::option<std::reference_wrapper<VirtualFile>> add_file(const VirtualFileInfo &info) {
        auto data = std::vector<u8>(info.data.size());
        std::memcpy(data.data(), info.data.data(), info.data.size());
        auto it = files.emplace(std::string(info.path), VirtualFile{ std::move(data) });
        return it.first->second;
    }

    ls::option<std::reference_wrapper<VirtualFile>> read_file(const std::string &virtual_path, const fs::path &real_path) {
        auto file_contents = File::to_bytes(real_path);
        if (file_contents.empty()) {
            // john loguru hates wstring
            std::string str = real_path.string();
            LOG_ERROR("Failed to read file '{}'!", str);
            return ls::nullopt;
        }

        auto it = files.emplace(virtual_path, VirtualFile{ std::move(file_contents) });
        return it.first->second;
    }
};

}  // namespace lr
