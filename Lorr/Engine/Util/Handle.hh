#pragma once

#include <type_traits>

namespace lr {
#define LR_HANDLE(name, type) \
    enum class name : type { Invalid = std::numeric_limits<type>::max() }

template<typename T>
bool is_handle_valid(T t)
{
    return t != T::Invalid;
}

template<typename T>
std::underlying_type_t<T> get_handle_val(T t)
{
    return static_cast<std::underlying_type_t<T>>(t);
}

template<typename HandleT, typename ValT>
HandleT set_handle_val(ValT v)
{
    return static_cast<HandleT>(v);
}
}  // namespace lr