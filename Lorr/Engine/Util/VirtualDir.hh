#pragma once

#include "OS/OS.hh"

namespace lr {
struct VirtualFile {
    std::vector<u8> contents = {};

    const u8 *data() const { return contents.data(); }
    const char *c_str() const { return reinterpret_cast<const char *>(contents.data()); }
    usize size() const { return contents.size(); }
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

    const auto &files() const { return m_files; }

    ankerl::unordered_dense::map<std::string, VirtualFile> m_files = {};
};
}  // namespace lr
