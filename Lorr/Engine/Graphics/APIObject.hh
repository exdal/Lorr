#pragma once

namespace lr::Graphics
{
template<typename T>
struct ToVKObjectType
{
};

#define LR_ASSIGN_OBJECT_TYPE(_type, _val)                \
    template<>                                            \
    struct ToVKObjectType<_type>                          \
    {                                                     \
        constexpr static u32 type = _val;                 \
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

}  // namespace lr::Graphics