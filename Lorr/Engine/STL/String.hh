// Created on Friday May 19th 2023 by exdal
// Last modified on Monday May 22nd 2023 by exdal

#pragma once

namespace ls
{
constexpr eastl::string_view TrimFileName(eastl::string_view directory)
{
    uptr namePos = directory.find_last_of("\\/");
    if (namePos == -1)
        return directory;

    return directory.substr(namePos + 1, directory.length() - namePos);
}

constexpr eastl::string_view TrimString(eastl::string_view str, eastl::string_view chars)
{
    u64 begin = str.find_first_not_of(chars);
    if (begin != -1)
        return str.substr(begin, str.find_last_not_of(chars) - begin + 1);

    return "";
}

constexpr eastl::string_view TrimQuotes(eastl::string_view str)
{
    if (str.empty())
        return str;

    if (*str.begin() == '\"' && *str.rbegin() == '\"')
        return str.substr(1, str.length() - 2);

    return str;
}

constexpr bool GetLine(eastl::string_view str, eastl::string_view &line)
{
    constexpr eastl::string_view kTrimChars = "\n\r";

    u64 offset = 0;
    if (!line.empty())
        offset = line.end() - str.begin();

    u64 beginOff = str.find_first_not_of(kTrimChars, offset);
    if (beginOff != -1)
    {
        u64 endOff = str.find_first_of(kTrimChars, beginOff);
        line = str.substr(beginOff, endOff - offset - (beginOff - offset));
        line = TrimString(line, kTrimChars);

        return true;
    }

    return false;
}
}  // namespace ls