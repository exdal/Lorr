#pragma once

#include <filesystem>

namespace lr::asset {
struct VirtualFile {
    std::vector<u8> data;
    std::chrono::file_clock::time_point touch_tp;
};

struct VirtualDirectory {
    void mkdir(std::string_view name);
    ankerl::unordered_dense::set<std::string, VirtualFile> m_files = {};
};

}  // namespace lr::asset
