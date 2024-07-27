#pragma once

#include "Engine/OS/OS.hh"

#include <ankerl/unordered_dense.h>

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
    VirtualDir() = default;
    VirtualDir(ls::span<VirtualFileInfo> infos) {
        for (const VirtualFileInfo &info : infos) {
            std::vector<u8> data = { info.data.begin(), info.data.end() };
            m_files.emplace(std::string(info.path), std::move(data));
        }
    }

    bool read_file(std::string_view virtual_path, std::string_view real_path) {
        File file(real_path, FileAccess::Read);
        if (!file) {
            LR_LOG_ERROR("Failed to read file!");
            return false;
        }

        std::vector<u8> contents(file.size);
        file.read(contents.data(), { 0, ~0_sz });
        m_files.emplace(std::string(virtual_path), std::move(contents));
        return true;
    }

    const auto &files() const { return m_files; }

    ankerl::unordered_dense::map<std::string, VirtualFile> m_files = {};
};

}  // namespace lr
