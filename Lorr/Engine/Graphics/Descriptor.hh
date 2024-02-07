#pragma once

#include "APIObject.hh"
#include "Common.hh"

#include "Pipeline.hh"

namespace lr::Graphics {
struct DescriptorLayoutElement {
    DescriptorLayoutElement() = default;
    DescriptorLayoutElement(
        u32 binding, DescriptorType type, ShaderStage stage, u32 max_descriptors, DescriptorBindingFlag flags = DescriptorBindingFlag::None);

    VkDescriptorSetLayoutBinding m_binding_info = {};
    DescriptorBindingFlag m_binding_flag = {};

    auto get_type() { return static_cast<DescriptorType>(m_binding_info.descriptorType); }
    auto get_count() { return m_binding_info.descriptorCount; }
};

struct DescriptorSetLayout {
    u32 m_max_descriptor_elements = 0;

    VkDescriptorSetLayout m_handle = VK_NULL_HANDLE;
};
LR_ASSIGN_OBJECT_TYPE(DescriptorSetLayout, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT);

struct ImageView;
struct ImageDescriptorInfo : VkDescriptorImageInfo {
    ImageDescriptorInfo(ImageView *image_view, ImageLayout layout);
};

struct Buffer;
struct BufferDescriptorInfo : VkDescriptorBufferInfo {
    BufferDescriptorInfo(Buffer *buffer, u64 size = ~0, u64 offset = 0);
};

struct Sampler;
struct SamplerDescriptorInfo : VkDescriptorImageInfo {
    SamplerDescriptorInfo(Sampler *sampler);
};

struct DescriptorGetInfo {
    DescriptorGetInfo(Buffer *buffer, u64 data_size, DescriptorType type);
    DescriptorGetInfo(ImageView *image_view, DescriptorType type);
    DescriptorGetInfo(Sampler *sampler);

    struct BufferRange {
        u64 address = 0;
        u64 data_size = 0;
    };

    union {
        BufferRange m_buffer_range = {};
        ImageView *m_image_view;
        Sampler *m_sampler;
        bool m_has_descriptor;
    };

    DescriptorType m_type = DescriptorType::Sampler;
};

struct DescriptorPoolSize : VkDescriptorPoolSize {
    DescriptorPoolSize() = default;
    DescriptorPoolSize(DescriptorType type, u32 count);
};

struct DescriptorSet {
    VkDescriptorSet m_handle = VK_NULL_HANDLE;
};
LR_ASSIGN_OBJECT_TYPE(DescriptorSet, VK_OBJECT_TYPE_DESCRIPTOR_SET);

LR_HANDLE(DescriptorSetID, usize);
struct Device;
struct DescriptorPool {
    DescriptorPoolFlag m_flags = DescriptorPoolFlag::None;
    VkDescriptorPool m_handle = VK_NULL_HANDLE;
};
LR_ASSIGN_OBJECT_TYPE(DescriptorPool, VK_OBJECT_TYPE_DESCRIPTOR_POOL);

struct DescriptorManager {
    DescriptorManager &begin(Device *device, u32 frame_count);
    DescriptorManager &add_set(DescriptorLayoutElement element, u32 count, DescriptorSetID &result);
    void build();

    eastl::span<DescriptorSet> get_arranged_sets(u32 slice_offset);
    PipelineLayout *get_layout(usize push_constant_size) { return &m_pipeline_layouts[push_constant_size]; }
    usize get_slice_size() { return m_descriptor_set_ranges.size(); }  // probably the correct size per frame

    struct DescriptorSetRange {
        DescriptorLayoutElement element = {};
        usize start = 0;
        usize count = 0;
    };

    u32 m_frame_count = 0;
    eastl::vector<DescriptorSetRange> m_descriptor_set_ranges = {};
    eastl::vector<DescriptorSet> m_descriptor_sets = {};
    // re-arranged descriptor sets, cache friendly and easy to just `span<T>`
    eastl::vector<DescriptorSet> m_arranged_sets = {};
    DescriptorPool m_pool = {};

    // 32 u32's and a layout that doesn't have push constants
    eastl::array<PipelineLayout, 32 + 1> m_pipeline_layouts = {};
    Device *m_device = nullptr;
};

struct WriteDescriptorSet : VkWriteDescriptorSet {
    WriteDescriptorSet() = default;
    WriteDescriptorSet(
        DescriptorSet *descriptor_set, ImageDescriptorInfo &image_info, DescriptorType dst_type, u32 dst_binding, u32 dst_element, u32 dst_count);
    WriteDescriptorSet(
        DescriptorSet *descriptor_set, BufferDescriptorInfo &buffer_info, DescriptorType dst_type, u32 dst_binding, u32 dst_element, u32 dst_count);
    WriteDescriptorSet(
        DescriptorSet *descriptor_set, SamplerDescriptorInfo &sampler_info, DescriptorType dst_type, u32 dst_binding, u32 dst_element, u32 dst_count);

private:
    WriteDescriptorSet(DescriptorSet *descriptor_set, DescriptorType dst_type, u32 dst_binding, u32 dst_element, u32 dst_count);
};

struct CopyDescriptorSet : VkCopyDescriptorSet {
    CopyDescriptorSet(DescriptorSet *src, u32 src_binding, u32 src_element, DescriptorSet *dst, u32 dst_binding, u32 dst_element, u32 count);
};
}  // namespace lr::Graphics
