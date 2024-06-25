#pragma once

#include "Common.hh"

#include "Engine/Crypt/FNV.hh"

namespace lr {
/////////////////////////////////
// BUFFERS

struct BufferInfo {
    BufferUsage usage_flags = BufferUsage::Vertex;
    MemoryFlag flags = MemoryFlag::None;
    MemoryPreference preference = MemoryPreference::Auto;
    u64 data_size = 0;

    std::string_view debug_name = {};
};

struct Buffer {
    Buffer() = default;
    Buffer(u64 data_size_, void *host_data_, u64 device_address_, VmaAllocation allocation_, VkBuffer buffer_)
        : data_size(data_size_),
          host_data(host_data_),
          device_address(device_address_),
          allocation(allocation_),
          handle(buffer_)
    {
    }

    u64 data_size = 0;
    void *host_data = nullptr;
    u64 device_address = 0;
    VmaAllocation allocation = {};
    VkBuffer handle = nullptr;

    constexpr static auto OBJECT_TYPE = VK_OBJECT_TYPE_BUFFER;
    operator auto &() { return handle; }
    explicit operator bool() { return handle != nullptr; }
};

/////////////////////////////////
// IMAGES

struct ImageInfo {
    ImageUsage usage_flags = ImageUsage::Sampled;
    Format format = Format::Unknown;
    ImageType type = ImageType::View2D;
    ImageLayout initial_layout = ImageLayout::Undefined;

    Extent3D extent = {};
    u32 slice_count = 1;
    u32 mip_levels = 1;
    ls::span<u32> queue_indices = {};

    std::string_view debug_name = {};
};

struct Image {
    Image() = default;
    Image(Format format_, Extent3D extent_, u32 slice_count_, u32 mip_count_, VmaAllocation allocation_, VkImage image_)
        : format(format_),
          extent(extent_),
          slice_count(slice_count_),
          mip_levels(mip_count_),
          allocation(allocation_),
          handle(image_)
    {
    }

    Format format = Format::Unknown;
    Extent3D extent = {};
    u32 slice_count = 1;
    u32 mip_levels = 1;

    VmaAllocation allocation = {};
    VkImage handle = VK_NULL_HANDLE;

    constexpr static auto OBJECT_TYPE = VK_OBJECT_TYPE_IMAGE;
    operator auto &() { return handle; }
    explicit operator bool() { return handle != nullptr; }
};

// Image views can have different formats depending on the creation flags
// Currently, we do not support it so view's format is automatically parent
// image format. See:
// https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkImageViewCreateInfo.html

struct ImageViewInfo {
    ImageID image_id = ImageID::Invalid;
    ImageUsage usage_flags = ImageUsage::None;
    ImageViewType type = ImageViewType::View2D;
    ImageSubresourceRange subresource_range = {};
    ImageComponentSwizzle swizzle_r = ImageComponentSwizzle::Identity;
    ImageComponentSwizzle swizzle_g = ImageComponentSwizzle::Identity;
    ImageComponentSwizzle swizzle_b = ImageComponentSwizzle::Identity;
    ImageComponentSwizzle swizzle_a = ImageComponentSwizzle::Identity;
    std::string_view debug_name = {};
};

struct ImageView {
    ImageView() = default;
    ImageView(Format format_, ImageViewType type_, const ImageSubresourceRange &subres_range_, VkImageView image_view_)
        : format(format_),
          type(type_),
          subresource_range(subres_range_),
          handle(image_view_)
    {
    }

    Format format = Format::Unknown;
    ImageViewType type = ImageViewType::View2D;
    ImageSubresourceRange subresource_range = {};

    VkImageView handle = VK_NULL_HANDLE;

    constexpr static auto OBJECT_TYPE = VK_OBJECT_TYPE_IMAGE_VIEW;
    operator auto &() { return handle; }
    explicit operator bool() { return handle != nullptr; }
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
    std::string_view debug_name = {};
};

struct Sampler {
    Sampler() = default;
    Sampler(VkSampler sampler_)
        : handle(sampler_)
    {
    }

    VkSampler handle = VK_NULL_HANDLE;

    constexpr static auto OBJECT_TYPE = VK_OBJECT_TYPE_SAMPLER;
    operator auto &() { return handle; }
    explicit operator bool() { return handle != nullptr; }
};

// Helper hash function for 'cached samplers'
constexpr SamplerHash HSAMPLER(SamplerInfo info)
{
    constexpr usize size = sizeof(SamplerInfo);
    auto v = std::bit_cast<std::array<const char, size>>(info);
    return static_cast<SamplerHash>(Hash::FNV64(v.data(), size));
}

}  // namespace lr
