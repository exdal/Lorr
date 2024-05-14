#pragma once

#include "OS/OS.hh"

#include "Util/VirtualDir.hh"

namespace lr::example {
static inline VirtualDir &default_vdir()
{
    static VirtualFileInfo vfile_infos[] = {
        { LR_SHADER_STD_FILE_PATH, "lorr" },
    };
    static VirtualDir vdir(std::span{ vfile_infos });

    return vdir;
}

static inline Result<std::string, os::FileResult> load_file(std::string_view path)
{
    std::string code;
    auto [file, result] = os::open_file(path, os::FileAccess::Read);
    if (!result) {
        return result;
    }

    code.resize(os::file_size(file));
    os::read_file(file, code.data(), { 0, ~0u });
    os::close_file(file);

    return std::move(code);
}

}  // namespace lr::example
