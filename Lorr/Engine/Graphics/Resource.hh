#pragma once

#include "APIObject.hh"
#include "Common.hh"

namespace lr::Graphics
{
/////////////////////////////////
// BUFFERS

struct BufferDesc
{
    BufferUsage m_usage_flags = BufferUsage::Vertex;
    u32 m_stride = 1;
    u64 m_data_size = 0;
};

struct Buffer
{
    u32 m_stride = 1;
    u64 m_device_address = 0;

    u64 m_allocator_data = ~0;
    VkBuffer m_handle = nullptr;

    operator VkBuffer &() { return m_handle; }
    explicit operator bool() { return m_handle != nullptr; }
};
LR_ASSIGN_OBJECT_TYPE(Buffer, VK_OBJECT_TYPE_BUFFER);

/////////////////////////////////
// IMAGES

struct ImageDesc
{
    ImageUsage m_usage_flags = ImageUsage::Sampled;
    Format m_format = Format::Unknown;
    ImageType m_type = ImageType::View2D;

    u32 m_width = 0;
    u32 m_height = 0;
    u32 m_slice_count = 1;
    u32 m_mip_levels = 1;
    eastl::span<u32> m_queue_indices = {};

    u64 m_data_size = 0;
};

struct Image : Tracked<VkImage>
{
    void init(Format format, u32 width, u32 height, u32 slice_count, u32 mip_levels)
    {
        m_format = format;
        m_width = width;
        m_height = height;
        m_slice_count = slice_count;
        m_mip_levels = mip_levels;
    }

    Format m_format = Format::Unknown;
    u32 m_width = 0;
    u32 m_height = 0;
    u32 m_slice_count = 1;
    u32 m_mip_levels = 1;

    bool m_swap_chain_image = false;
    u64 m_allocator_data = ~0;
};
LR_ASSIGN_OBJECT_TYPE(Image, VK_OBJECT_TYPE_IMAGE);

// Image views can have different formats depending on the creation flags
// Currently, we do not support it so view's format is automatically parent
// image format. See:
// https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkImageViewCreateInfo.html

struct ImageSubresourceInfo
{
    ImageAspect m_aspect_mask = ImageAspect::Color;
    u32 m_base_mip : 8 = 0;
    u32 m_mip_count : 8 = 1;
    u32 m_base_slice : 8 = 0;
    u32 m_slice_count : 8 = 1;
};

struct ImageViewDesc
{
    Image *m_image = nullptr;
    ImageViewType m_type = ImageViewType::View2D;
    // Component mapping
    ImageComponentSwizzle m_swizzle_r = ImageComponentSwizzle::Identity;
    ImageComponentSwizzle m_swizzle_g = ImageComponentSwizzle::Identity;
    ImageComponentSwizzle m_swizzle_b = ImageComponentSwizzle::Identity;
    ImageComponentSwizzle m_swizzle_a = ImageComponentSwizzle::Identity;
    ImageSubresourceInfo m_subresource_info = {};
};

struct ImageSubresourceRange : VkImageSubresourceRange
{
    ImageSubresourceRange(ImageSubresourceInfo info);
};

struct ImageView : Tracked<VkImageView>
{
    Format m_format = Format::Unknown;
    ImageViewType m_type = ImageViewType::View2D;
    ImageSubresourceInfo m_subresource_info = {};
};
LR_ASSIGN_OBJECT_TYPE(ImageView, VK_OBJECT_TYPE_IMAGE_VIEW);

/////////////////////////////////
// SAMPLERS

struct SamplerDesc
{
    Filtering m_min_filter : 1;
    Filtering m_mag_filter : 1;
    Filtering m_mip_filter : 1;
    TextureAddressMode m_address_u : 2;
    TextureAddressMode m_address_v : 2;
    TextureAddressMode m_address_w : 2;
    u32 m_use_anisotropy : 1;
    CompareOp m_compare_op : 3;
    float m_max_anisotropy = 0;
    float m_mip_lod_bias = 0;
    float m_min_lod = 0;
    float m_max_lod = 0;
};

struct Sampler : Tracked<VkSampler>
{
};
LR_ASSIGN_OBJECT_TYPE(Sampler, VK_OBJECT_TYPE_SAMPLER);

/////////////////////////////////
// DESCRIPTORS

struct DescriptorLayoutElement
{
    DescriptorLayoutElement(
        u32 binding, DescriptorType type, ShaderStage stage, u32 array_size = 1, DescriptorBindingFlag flags = DescriptorBindingFlag::None);
    VkDescriptorSetLayoutBinding m_binding_info = {};
    DescriptorBindingFlag m_binding_flag = {};
};

using DescriptorSetLayout = VkDescriptorSetLayout;
LR_ASSIGN_OBJECT_TYPE(DescriptorSetLayout, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT);

struct DescriptorGetInfo
{
    DescriptorGetInfo() = default;
    DescriptorGetInfo(Buffer *buffer, u64 data_size, DescriptorType type);
    DescriptorGetInfo(ImageView *image_view, DescriptorType type);
    DescriptorGetInfo(Sampler *sampler);

    union
    {
        struct
        {
            u64 m_buffer_address = 0;
            u64 m_data_size = 0;
        };
        ImageView *m_image_view;
        Sampler *m_sampler;
        bool m_has_descriptor;
    };

    DescriptorType m_type = DescriptorType::Sampler;
};

struct DescriptorPoolSize : VkDescriptorPoolSize
{
    DescriptorPoolSize() = default;
    DescriptorPoolSize(DescriptorType type, u32 count);
};

struct DescriptorSet : Tracked<VkDescriptorSet>
{
};

struct DescriptorPool : Tracked<VkDescriptorPool>
{
};

}  // namespace lr::Graphics