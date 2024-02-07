#pragma once

#include "Common.hh"

namespace lr::Graphics {
template<typename T>
struct ToVKObjectType;

#define LR_ASSIGN_OBJECT_TYPE(_type, _val)                 \
    template<>                                             \
    struct ToVKObjectType<_type> {                         \
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
    struct VKStruct<vk_struct> {                                \
        constexpr static VkStructureType type = structure_type; \
    };

LR_DEFINE_VK_TYPE(VkPhysicalDeviceProperties2, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2);
LR_DEFINE_VK_TYPE(VkPhysicalDeviceMemoryProperties2, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2);
LR_DEFINE_VK_TYPE(VkPhysicalDeviceMemoryBudgetPropertiesEXT, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT);
LR_DEFINE_VK_TYPE(VkPhysicalDeviceDescriptorBufferPropertiesEXT, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_PROPERTIES_EXT);

}  // namespace lr::Graphics
