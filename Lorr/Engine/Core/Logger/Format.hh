#pragma once

#include <fmt/format.h>
#include <EASTL/string.h>

namespace fmt
{
template<>
struct formatter<eastl::string_view> : formatter<string_view>
{
    template<typename FormatContext>
    constexpr auto format(eastl::string_view p, FormatContext &ctx) const
    {
        return formatter<string_view>::format({ p.data(), p.length() }, ctx);
    }
};

template<>
struct formatter<eastl::string> : formatter<string_view>
{
    template<typename FormatContext>
    constexpr auto format(const eastl::string &p, FormatContext &ctx) const
    {
        return formatter<string_view>::format(p.c_str(), ctx);
    }
};
}  // namespace fmt

namespace eastl
{
template<typename... T>
string format(fmt::string_view sv, T &&...args)
{
    auto buffer = fmt::memory_buffer();
    fmt::detail::vformat_to(buffer, sv, fmt::make_format_args(args...));
    fmt::detail::assume(buffer.size() < basic_string<char>().max_size());
    return { buffer.data(), buffer.size() };
}
}  // namespace eastl
