#pragma once

#include "OS/OS.hh"

namespace lr {
struct VirtualFile {
    std::vector<u8> contents = {};
};

struct VirtualDir {
    bool read_file(std::string_view real_path, const std::string &virtual_path)
    {
        auto [file, file_result] = os::open_file(real_path, os::FileAccess::Read);
        if (!file_result) {
            LR_LOG_ERROR("Failed to read file!");
            return false;
        }

        auto [file_size, _] = os::file_size(file);
        std::vector<u8> file_contents(file_size);
        os::read_file(file, file_contents.data(), { 0, file_size });
        os::close_file(file);

        m_files.emplace(virtual_path, std::move(file_contents));
        return true;
    }

    ankerl::unordered_dense::map<std::string, VirtualFile> m_files = {};
};
}  // namespace lr
