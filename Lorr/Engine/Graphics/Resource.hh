#pragma once

#include "Common.hh"

namespace lr::graphics {
/////////////////////////////////
// BUFFERS

struct BufferInfo {
    BufferUsage usage_flags = BufferUsage::Vertex;
    u64 data_size = 0;
};

struct Buffer {
    Buffer() = default;
    Buffer(VkBuffer buffer, VmaAllocation allocation)
        : m_handle(buffer),
          m_allocation(allocation)
    {
    }

    VmaAllocation m_allocation = {};
    VkBuffer m_handle = nullptr;

    constexpr static auto OBJECT_TYPE = VK_OBJECT_TYPE_BUFFER;
    operator auto &() { return m_handle; }
    explicit operator bool() { return m_handle != nullptr; }
};

/////////////////////////////////
// IMAGES

struct ImageInfo {
    ImageUsage usage_flags = ImageUsage::Sampled;
    Format format = Format::Unknown;
    ImageType type = ImageType::View2D;

    Extent3D extent = {};
    u32 slice_count = 1;
    u32 mip_levels = 1;
    std::span<u32> queue_indices = {};

    u64 data_size = 0;
};

struct Image {
    Image() = default;
    Image(
        VkImage image,
        VmaAllocation allocation,
        Format format,
        Extent3D extent,
        u32 slice_count,
        u32 mip_count)
        : m_handle(image),
          m_allocation(allocation),
          m_format(format),
          m_extent(extent),
          m_slice_count(slice_count),
          m_mip_levels(mip_count)
    {
    }

    Format m_format = Format::Unknown;
    Extent3D m_extent = {};
    u32 m_slice_count = 1;
    u32 m_mip_levels = 1;

    VmaAllocation m_allocation = {};
    VkImage m_handle = VK_NULL_HANDLE;

    constexpr static auto OBJECT_TYPE = VK_OBJECT_TYPE_IMAGE;
    operator auto &() { return m_handle; }
    explicit operator bool() { return m_handle != nullptr; }
};

// Image views can have different formats depending on the creation flags
// Currently, we do not support it so view's format is automatically parent
// image format. See:
// https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkImageViewCreateInfo.html

struct ImageViewInfo {
    Image *image = nullptr;
    ImageViewType type = ImageViewType::View2D;
    // Component mapping
    ImageComponentSwizzle swizzle_r = ImageComponentSwizzle::Identity;
    ImageComponentSwizzle swizzle_g = ImageComponentSwizzle::Identity;
    ImageComponentSwizzle swizzle_b = ImageComponentSwizzle::Identity;
    ImageComponentSwizzle swizzle_a = ImageComponentSwizzle::Identity;
    ImageSubresourceRange subresource_range = {};
};

struct ImageView {
    ImageView() = default;
    ImageView(
        VkImageView image_view, Format format, ImageViewType type, const ImageSubresourceRange &subres_range)
        : m_handle(image_view),
          m_format(format),
          m_type(type),
          m_subresource_range(subres_range)
    {
    }

    Format m_format = Format::Unknown;
    ImageViewType m_type = ImageViewType::View2D;
    ImageSubresourceRange m_subresource_range = {};

    VkImageView m_handle = VK_NULL_HANDLE;

    constexpr static auto OBJECT_TYPE = VK_OBJECT_TYPE_IMAGE_VIEW;
    operator auto &() { return m_handle; }
    explicit operator bool() { return m_handle != nullptr; }
};

/////////////////////////////////
// SAMPLERS

struct SamplerInfo {
    Filtering min_filter = {};
    Filtering mag_filter = {};
    Filtering mip_filter = {};
    TextureAddressMode address_u = {};
    TextureAddressMode address_v = {};
    TextureAddressMode address_w = {};
    CompareOp compare_op = {};
    float max_anisotropy = 0;
    float mip_lod_bias = 0;
    float min_lod = 0;
    float max_lod = 0;
    bool use_anisotropy = false;
};

struct Sampler {
    Sampler() = default;
    Sampler(VkSampler sampler)
        : m_handle(sampler)
    {
    }

    VkSampler m_handle = VK_NULL_HANDLE;

    constexpr static auto OBJECT_TYPE = VK_OBJECT_TYPE_SAMPLER;
    operator auto &() { return m_handle; }
    explicit operator bool() { return m_handle != nullptr; }
};

}  // namespace lr::graphics
