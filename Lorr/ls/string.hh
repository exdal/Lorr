#pragma once

namespace ls {
constexpr std::string escape_str(std::string_view str) {
    std::string r;
    r.reserve(str.size());

    for (c8 c : str) {
        switch (c) {
                // clang-format off
            case '\'': r += "\\\'"; break;
            case '\"': r += "\\\""; break;
            case '\?': r += "\\?";  break;
            case '\\': r += "\\\\"; break;
            case '\a': r += "\\a";  break;
            case '\b': r += "\\b";  break;
            case '\f': r += "\\f";  break;
            case '\n': r += "\\n";  break;
            case '\r': r += "\\r";  break;
            case '\t': r += "\\t";  break;
            case '\v': r += "\\v";  break;
            default: r += c; break;
                // clang-format on
        }
    }

    return r;
}
} // namespace ls
