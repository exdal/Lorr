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
    std::string_view real_path;
    std::string_view virtual_path;
};

struct VirtualDir {
    VirtualDir() = default;
    VirtualDir(std::span<VirtualFileInfo> infos)
    {
        for (auto &info : infos) {
            if (!read_file(info)) {
                LR_LOG_ERROR("Failed to load file '{}' for VFile '{}'!", info.real_path, info.virtual_path);
            }
        }
    }

    bool read_file(const VirtualFileInfo &info)
    {
        auto [file, file_result] = os::open_file(info.real_path, os::FileAccess::Read);
        if (!file_result) {
            LR_LOG_ERROR("Failed to read file!");
            return false;
        }

        auto [file_size, _] = os::file_size(file);
        std::vector<u8> file_contents(file_size);
        os::read_file(file, file_contents.data(), { 0, file_size });
        os::close_file(file);

        m_files.emplace(std::string(info.virtual_path), std::move(file_contents));
        return true;
    }

    const auto &files() const { return m_files; }

    ankerl::unordered_dense::map<std::string, VirtualFile> m_files = {};
};
}  // namespace lr
