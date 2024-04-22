#pragma once

#include "Common.hh"

namespace lr::graphics {

struct DescriptorPoolInfo {
    std::span<DescriptorPoolSize> sizes = {};
    u32 max_sets = 0;
    DescriptorPoolFlag flags = DescriptorPoolFlag::None;
};

struct DescriptorPool {
    DescriptorPool() = default;
    DescriptorPool(VkDescriptorPool pool, DescriptorPoolFlag flags)
        : m_handle(pool),
          m_flags(flags)
    {
    }

    DescriptorPoolFlag m_flags = DescriptorPoolFlag::None;

    VkDescriptorPool m_handle = VK_NULL_HANDLE;

    constexpr static auto OBJECT_TYPE = VK_OBJECT_TYPE_DESCRIPTOR_POOL;
    operator auto &() { return m_handle; }
    explicit operator bool() { return m_handle != nullptr; }
};

struct DescriptorSetLayoutInfo {
    std::span<DescriptorSetLayoutElement> elements = {};
    std::span<DescriptorBindingFlag> binding_flags = {};
    DescriptorSetLayoutFlag flags = DescriptorSetLayoutFlag::None;
};

struct DescriptorSetLayout {
    DescriptorSetLayout() = default;
    DescriptorSetLayout(VkDescriptorSetLayout descriptor_set_layout)
        : m_handle(descriptor_set_layout)
    {
    }

    VkDescriptorSetLayout m_handle = VK_NULL_HANDLE;

    constexpr static auto OBJECT_TYPE = VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT;
    operator auto &() { return m_handle; }
    explicit operator bool() { return m_handle != nullptr; }
};

struct DescriptorSetInfo {
    DescriptorSetLayout &layout;
    DescriptorPool &pool;
    u32 descriptor_count = 0;
};

struct DescriptorSet {
    DescriptorSet() = default;
    DescriptorSet(VkDescriptorSet descriptor_set, DescriptorPool *pool)
        : m_handle(descriptor_set),
          m_pool(pool)
    {
    }

    DescriptorPool *m_pool = nullptr;
    VkDescriptorSet m_handle = VK_NULL_HANDLE;

    constexpr static auto OBJECT_TYPE = VK_OBJECT_TYPE_DESCRIPTOR_SET;
    operator auto &() { return m_handle; }
    explicit operator bool() { return m_handle != nullptr; }
};

}  // namespace lr::graphics
