#include "String.hh"

#include <locale>

eastl::wstring ls::to_wstring(eastl::string_view str)
{
    eastl::wstring result = {};
    result.resize(str.length());

    std::use_facet<std::ctype<wchar_t>>(std::locale()).widen(str.data(), str.data() + str.size(), result.data());
    return result;
}