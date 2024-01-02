#pragma once

namespace lr::Graphics
{
template<typename T>
struct ToVKObjectType
{
};

#define LR_ASSIGN_OBJECT_TYPE(_type, _val)                 \
    template<>                                             \
    struct ToVKObjectType<_type>                           \
    {                                                      \
        constexpr static u32 type = _val;                  \
        constexpr static eastl::string_view name = #_type; \
    };

template<typename T>
static bool validate_handle(T *var)
{
    assert(var);

    if (var)
        return true;

    LOG_CRITICAL("Passing null {} in Graphics::Device.", ToVKObjectType<T>::type);
    return false;
}

template<typename T>
constexpr auto type_name()
{
#ifdef __clang__
    return __PRETTY_FUNCTION__;
#elif defined(__GNUC__)
    return __PRETTY_FUNCTION__;
#elif defined(_MSC_VER)
    return __FUNCSIG__;
#else
#error "This compiler for type_name not implemented"
#endif
}

template<typename HandleT>
struct Tracked
{
    constexpr static eastl::string_view __handle_name = type_name<HandleT>();

    Tracked() = default;
    ~Tracked()
    {
        if (m_handle != 0)
            LOG_TRACE("Tracked object'{}' destoryed but native handle is still alive!", __handle_name);
    }

    HandleT m_handle = 0;
    operator HandleT &() { return m_handle; }
    explicit operator bool() { return m_handle != 0; }
};

}  // namespace lr::Graphics