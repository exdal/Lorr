
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
        auto format(eastl::string_view p, FormatContext &ctx) -> decltype(ctx.out())
        {
            return format_to(ctx.out(), "{:{}}", p.data(), p.size());
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
        auto format(const eastl::string &p, FormatContext &ctx) -> decltype(ctx.out())
        {
            return format_to(ctx.out(), "{:{}}", p.c_str(), p.length());
        }
    };

}  // namespace fmt

namespace lr
{
    template<typename... T>
    inline eastl::string Format(fmt::basic_string_view<char> fmt, T &&...args)
    {
        auto buffer = fmt::memory_buffer();
        fmt::detail::vformat_to(buffer, fmt, fmt::make_format_args(args...));

        auto size = buffer.size();
        fmt::detail::assume(size < std::basic_string<char>().max_size());

        return eastl::basic_string<char>(buffer.data(), size);
    }

}  // namespace lr
