// Created on Friday May 6th 2022 by exdal
// Last modified on Monday May 22nd 2023 by exdal
//
// Created on Friday 6th May 2022 by exdal
//

#pragma once

#include <fmt/format.h>

namespace fmt
{
template<>
struct formatter<eastl::string_view>
{
    template<typename ParseContext>
    constexpr auto parse(ParseContext &ctx)
    {
        return ctx.begin();
    }

    template<typename FormatContext>
    constexpr auto format(eastl::string_view p, FormatContext &ctx) -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), "{:{}}", p.data(), p.length());
    }
};

template<>
struct formatter<eastl::string>
{
    template<typename ParseContext>
    constexpr auto parse(ParseContext &ctx)
    {
        return ctx.begin();
    }

    template<typename FormatContext>
    constexpr auto format(const eastl::string &p, FormatContext &ctx) -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), "{:{}}", p.c_str(), p.length());
    }
};

}  // namespace fmt


namespace fmt
{
template<typename... T>
FMT_INLINE eastl::string format_eastl(string_view fmt, T &&...args)
{
    auto buffer = memory_buffer();
    detail::vformat_to(buffer, fmt, fmt::make_format_args(args...));

    auto size = buffer.size();
    detail::assume(size < eastl::basic_string<char>().max_size());
    return eastl::string(buffer.data(), buffer.size());
}

#define _FMT fmt::format_eastl

}  // namespace fmt
