#pragma once

namespace ls
{
struct CharRange
{
    u32 min;
    u32 max;
};

constexpr eastl::string_view trim_forward_ranged(eastl::string_view str, eastl::span<CharRange> ranges)
{
    for (usize i = 0; i < str.length(); i++)
    {
        i8 c = str[i];
        bool inRange = false;
        for (CharRange &range : ranges)
            if (c >= range.min && c <= range.max)
                inRange = true;

        if (!inRange)
            return str.substr(0, i);
    }

    return str;
}

constexpr eastl::string_view trim_file_name(eastl::string_view directory)
{
    usize namePos = directory.find_last_of("\\/");
    if (namePos == -1)
        return directory;

    return directory.substr(namePos + 1, directory.length() - namePos);
}

constexpr eastl::string_view trim_file_directory(eastl::string_view directory)
{
    usize namePos = directory.find_last_of("\\/");
    if (namePos == -1)
        return directory;

    return directory.substr(0, namePos);
}

constexpr eastl::string_view trim_string(eastl::string_view str, eastl::string_view chars)
{
    u64 begin = str.find_first_not_of(chars);
    if (begin != -1)
        return str.substr(begin, str.find_last_not_of(chars) - begin + 1);

    return "";
}

constexpr eastl::string_view trim_quotes(eastl::string_view str, char quote_begin = '\"', char quote_end = '\"')
{
    if (str.empty())
        return str;

    if (*str.begin() == quote_begin && *str.rbegin() == quote_end)
        return str.substr(1, str.length() - 2);

    return str;
}

constexpr bool get_line(eastl::string_view str, eastl::string_view &line)
{
    constexpr eastl::string_view kTrimChars = "\n\r";

    u64 offset = 0;
    if (!line.empty())
        offset = (u64)line.data() + line.length() - (u64)str.data();

    u64 begin_off = str.find_first_not_of(kTrimChars, offset);
    if (begin_off != -1)
    {
        u64 end_off = str.find_first_of(kTrimChars, begin_off);
        line = str.substr(begin_off, end_off - offset - (begin_off - offset));
        line = trim_string(line, kTrimChars);

        return true;
    }

    return false;
}

eastl::wstring to_wstring(eastl::string_view str);
}  // namespace ls