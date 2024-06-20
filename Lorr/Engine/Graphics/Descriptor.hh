#pragma once

#include "Common.hh"

namespace lr::graphics {

struct DescriptorPoolInfo {
    ls::span<DescriptorPoolSize> sizes = {};
    u32 max_sets = 0;
    DescriptorPoolFlag flags = DescriptorPoolFlag::None;
};

struct DescriptorPool {
    DescriptorPool() = default;
    DescriptorPool(DescriptorPoolFlag flags_, VkDescriptorPool pool_)
        : flags(flags_),
          handle(pool_)
    {
    }

    DescriptorPoolFlag flags = DescriptorPoolFlag::None;
    VkDescriptorPool handle = VK_NULL_HANDLE;

    constexpr static auto OBJECT_TYPE = VK_OBJECT_TYPE_DESCRIPTOR_POOL;
    operator auto &() { return handle; }
    explicit operator bool() { return handle != nullptr; }
};

struct DescriptorSetLayoutInfo {
    ls::span<DescriptorSetLayoutElement> elements = {};
    ls::span<DescriptorBindingFlag> binding_flags = {};
    DescriptorSetLayoutFlag flags = DescriptorSetLayoutFlag::None;
};

struct DescriptorSetLayout {
    DescriptorSetLayout() = default;
    DescriptorSetLayout(VkDescriptorSetLayout descriptor_set_layout_)
        : handle(descriptor_set_layout_)
    {
    }

    VkDescriptorSetLayout handle = VK_NULL_HANDLE;

    constexpr static auto OBJECT_TYPE = VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT;
    operator auto &() { return handle; }
    explicit operator bool() { return handle != nullptr; }
};

struct DescriptorSetInfo {
    DescriptorSetLayout &layout;
    DescriptorPool &pool;
    u32 descriptor_count = 0;
};

struct DescriptorSet {
    DescriptorSet() = default;
    DescriptorSet(DescriptorPool *pool_, VkDescriptorSet descriptor_set_)
        : pool(pool_),
          handle(descriptor_set_)
    {
    }

    DescriptorPool *pool = nullptr;
    VkDescriptorSet handle = VK_NULL_HANDLE;

    constexpr static auto OBJECT_TYPE = VK_OBJECT_TYPE_DESCRIPTOR_SET;
    operator auto &() { return handle; }
    explicit operator bool() { return handle != nullptr; }
};

}  // namespace lr::graphics
