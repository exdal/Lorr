#pragma once

namespace ls {
struct path_view {
    std::string_view str = {};
    usize extension_pos = ~0_sz;
    usize name_pos = ~0_sz;

    path_view(std::string_view path)
        : str(path)
    {
        name_pos = str.find_last_of("\\/");
        extension_pos = str.rfind('.');
    }

    std::optional<std::string_view> directory()
    {
        if (name_pos == ~0_sz) {
            return ".";
        }

        size_t pos = name_pos;
        if (pos == ~0_sz) {
            pos = str.find_last_of("\\/");
        }

        return str.substr(0, name_pos);
    }

    // Returns the file name with extension (if available)
    std::optional<std::string_view> name()
    {
        if (str.ends_with('\\') || str.ends_with('/')) {
            return std::nullopt;
        }

        return str.substr(name_pos + 1, str.length() - name_pos);
    }

    std::optional<std::string_view> name_only()
    {
        if (str.ends_with('\\') || str.ends_with('/')) {
            return std::nullopt;
        }

        auto n = str.substr(name_pos + 1, extension_pos - name_pos - 1);
        if (n.empty()) {
            return std::nullopt;
        }

        return n;
    }

    std::optional<std::string_view> extension()
    {
        if (extension_pos == ~0_sz) {
            return std::nullopt;
        }

        return str.substr(extension_pos);
    }

    std::string_view full() { return str; }
};
}  // namespace ls
