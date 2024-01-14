#pragma once

#include "Common.hh"

namespace lr::Graphics
{
template<typename T>
struct ToVKObjectType;

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
struct VKStruct;

#define LR_DEFINE_VK_TYPE(vk_struct, structure_type)            \
    template<>                                                  \
    struct VKStruct<vk_struct>                                  \
    {                                                           \
        constexpr static VkStructureType type = structure_type; \
    };

LR_DEFINE_VK_TYPE(VkPhysicalDeviceProperties2, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2);
LR_DEFINE_VK_TYPE(VkPhysicalDeviceMemoryProperties2, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2);
LR_DEFINE_VK_TYPE(VkPhysicalDeviceMemoryBudgetPropertiesEXT, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT);
LR_DEFINE_VK_TYPE(VkPhysicalDeviceDescriptorBufferPropertiesEXT, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_PROPERTIES_EXT);

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

template<typename HandleT, bool EnableLoggingT = true>
struct Tracked
{
    constexpr static eastl::string_view __handle_name = type_name<HandleT>();

    Tracked() = default;
    ~Tracked()
    {
        if (!EnableLoggingT)
            return;

        if (m_handle != 0)
            LOG_TRACE("Tracked object'{}' destoryed but native handle is still alive!", __handle_name);
    }

    HandleT m_handle = 0;
    operator HandleT &() { return m_handle; }
    explicit operator bool() { return m_handle != 0; }
};

}  // namespace lr::Graphics
