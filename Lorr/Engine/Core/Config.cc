#include <charconv>

#include "Config.hh"
#include "FileSystem.hh"
#include "STL/String.hh"

static ls::CharRange kVarRanges[] = { { 0x41, 0x5a }, { 0x30, 0x39 }, { 0x5f, 0x5f } };

namespace lr
{

constexpr static eastl::string_view kWhitespaces = " \t";

template<typename _T>
struct DataParser;

template<>
struct DataParser<eastl::string>
{
    eastl::string Parse(eastl::string_view str, eastl::string defaultVal)
    {
        ZoneScoped;

        str = ls::trim_string(str, kWhitespaces);
        for (usize i = 0; i < str.length(); i++)
        {
            i8 c = str[i];
            i8 lastC = str[i - 1];

            if (c == '"' && lastC != '\\')
            {
                if (i == 0)
                {
                    str.remove_prefix(1);
                }
                else
                {
                    str.remove_suffix(str.length() - i);
                    break;
                }
            }
        }

        return eastl::string(str);
    }
};

template<>
struct DataParser<u32>
{
    u32 Parse(eastl::string_view str, u32 defaultVal)
    {
        ZoneScoped;

        str = ls::trim_string(str, kWhitespaces);
        u32 result = 0;
        auto fc = std::from_chars(str.begin(), str.end(), result);
        if (fc.ec != std::errc())
            return defaultVal;

        return result;
    }
};

template<>
struct DataParser<u64>
{
    u64 Parse(eastl::string_view str, u64 defaultVal)
    {
        ZoneScoped;

        str = ls::trim_string(str, kWhitespaces);
        u64 result = 0;
        auto fc = std::from_chars(str.begin(), str.end(), result);
        if (fc.ec != std::errc())
            return defaultVal;

        return result;
    }
};

template<>
struct DataParser<f32>
{
    f32 Parse(eastl::string_view str, f32 defaultVal)
    {
        ZoneScoped;

        str = ls::trim_string(str, kWhitespaces);
        f32 result = 0.0;
        auto fc = std::from_chars(str.begin(), str.end(), result);
        if (fc.ec != std::errc())
            return defaultVal;

        return result;
    }
};

template<>
struct DataParser<f64>
{
    f64 Parse(eastl::string_view str, f64 defaultVal)
    {
        ZoneScoped;

        str = ls::trim_string(str, kWhitespaces);
        f64 result = 0.0;
        auto fc = std::from_chars(str.begin(), str.end(), result);
        if (fc.ec != std::errc())
            return defaultVal;

        return result;
    }
};

Config &Config::Get()
{
    static Config g_Config;
    return g_Config;
}

bool Config::Init()
{
    ZoneScoped;

    eastl::string data = fs::read_file("config.cfg");
    eastl::string_view line;
    while (ls::get_line(eastl::string_view(data), line))
        Config::Get().ParseLine(line);

    return true;
}

#define CONFIG_DEFINE_VAR(type, name, defVal)                                         \
    case CRC32HashOf(#name):                                                          \
        cfg_##name.Val = DataParser<type>().Parse(line.substr(var.length()), defVal); \
        break;

bool Config::ParseLine(eastl::string_view line)
{
    ZoneScoped;

    line = ls::trim_string(line, kWhitespaces);

    eastl::string_view var = ls::trim_forward_ranged(line, kVarRanges);
    switch (Hash::CRC32String(Hash::CRC32DataAligned, var))
    {
        _CONFIG_VAR_LIST;
        default:
            break;
    }

    return true;
}

}  // namespace lr