#pragma once

#include "Engine/Graphics/Slang/Compiler.hh"
#include <vuk/Buffer.hpp>
#include <vuk/ImageAttachment.hpp>
#include <vuk/Value.hpp>
#include <vuk/runtime/vk/Image.hpp>
#include <vuk/runtime/vk/Pipeline.hpp>
#include <vuk/runtime/vk/Query.hpp>
#include <vuk/runtime/vk/VkSwapchain.hpp>
#include <vuk/runtime/vk/VkTypes.hpp>

namespace lr {
enum class BufferID : u64 { Invalid = ~0_u64 };
enum class ImageID : u64 { Invalid = ~0_u64 };
enum class ImageViewID : u64 { Invalid = ~0_u64 };
enum class SamplerID : u64 { Invalid = ~0_u64 };
enum class PipelineID : u64 { Invalid = ~0_u64 };

/////////////////////////////////
// DEVICE RESOURCES
struct Device;

struct Buffer {
    [[nodiscard]] static auto create(Device &, u64 size, vuk::MemoryUsage memory_usage = vuk::MemoryUsage::eGPUonly, LR_THISCALL)
        -> std::expected<Buffer, vuk::VkException>;

    auto data_size() const -> u64;
    auto device_address() const -> u64;
    auto host_ptr() const -> u8 *;
    auto id() const -> BufferID;

    // if new_size is smaller than current size, this will do nothing
    [[nodiscard]] auto resize(Device &, u64 new_size, vuk::MemoryUsage memory_usage = vuk::MemoryUsage::eGPUonly, LR_THISCALL)
        -> std::expected<Buffer, vuk::VkException>;

    auto acquire(Device &, vuk::Name name, vuk::Access access, u64 offset = 0, u64 size = ~0_u64) -> vuk::Value<vuk::Buffer>;
    auto discard(Device &, vuk::Name name, u64 offset = 0, u64 size = ~0_u64) -> vuk::Value<vuk::Buffer>;
    auto subrange(Device &, u64 offset = 0, u64 size = ~0_u64) -> vuk::Buffer;

    explicit operator bool() const {
        return id_ != BufferID::Invalid;
    }

private:
    u64 data_size_ = 0;
    void *host_data_ = nullptr;
    u64 device_address_ = 0;
    BufferID id_ = BufferID::Invalid;
};

struct ImageView;
struct ImageInfo {
    vuk::Format format;
    vuk::ImageUsageFlags usage;
    vuk::ImageType type;
    vuk::Extent3D extent;
    u32 slice_count = 1;
    u32 mip_count = 1;
    std::string_view name = {};
};
struct Image {
    static auto create(Device &, const ImageInfo &info, LR_THISCALL) -> std::expected<Image, vuk::VkException>;
    static auto create_with_view(Device &, const ImageInfo &info, LR_THISCALL) -> std::expected<std::tuple<Image, ImageView>, vuk::VkException>;

    auto format() const -> vuk::Format;
    auto extent() const -> vuk::Extent3D;
    auto slice_count() const -> u32;
    auto mip_count() const -> u32;
    auto id() const -> ImageID;

    explicit operator bool() const {
        return id_ != ImageID::Invalid;
    }

private:
    vuk::Format format_ = vuk::Format::eUndefined;
    vuk::Extent3D extent_ = {};
    u32 slice_count_ = 1;
    u32 mip_levels_ = 1;
    ImageID id_ = ImageID::Invalid;
};

// Image views can have different formats depending on the creation
// flags Currently, we do not support it so view's format is
// automatically parent image format. See:
// https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkImageViewCreateInfo.html

struct ImageViewInfo {
    vuk::ImageUsageFlags image_usage;
    vuk::ImageViewType type;
    vuk::ImageSubresourceRange subresource_range;
    std::string_view name = {};
};

struct ImageView {
    static auto create(Device &, Image &image, const ImageViewInfo &info, LR_THISCALL) -> std::expected<ImageView, vuk::VkException>;

    auto to_attachment(Device &, const vuk::ImageUsageFlags &usage) const -> vuk::ImageAttachment;
    auto to_attachment(Device &, vuk::ImageAttachment::Preset preset) const -> vuk::ImageAttachment;
    auto discard(Device &, vuk::Name name, const vuk::ImageUsageFlags &usage, LR_THISCALL) const -> vuk::Value<vuk::ImageAttachment>;
    auto acquire(Device &, vuk::Name name, const vuk::ImageUsageFlags &usage, vuk::Access last_access, LR_THISCALL) const
        -> vuk::Value<vuk::ImageAttachment>;

    auto format() const -> vuk::Format;
    auto extent() const -> vuk::Extent3D;
    auto subresource_range() const -> vuk::ImageSubresourceRange;
    auto slice_count() const -> u32;
    auto mip_count() const -> u32;
    auto image_id() const -> ImageID;
    auto id() const -> ImageViewID;
    auto index() const -> u32;

    explicit operator bool() const {
        return id_ != ImageViewID::Invalid;
    }

private:
    vuk::Format format_ = vuk::Format::eUndefined;
    vuk::Extent3D extent_ = {};
    vuk::ImageViewType type_ = vuk::ImageViewType::e2D;
    vuk::ImageSubresourceRange subresource_range_ = {};
    ImageID bound_image_id_ = ImageID::Invalid;
    ImageViewID id_ = ImageViewID::Invalid;
};

struct SamplerInfo {
    vuk::Filter min_filter = vuk::Filter::eLinear;
    vuk::Filter mag_filter = vuk::Filter::eLinear;
    vuk::SamplerMipmapMode mipmap_mode = vuk::SamplerMipmapMode::eLinear;
    vuk::SamplerAddressMode addr_u = vuk::SamplerAddressMode::eRepeat;
    vuk::SamplerAddressMode addr_v = vuk::SamplerAddressMode::eRepeat;
    vuk::SamplerAddressMode addr_w = vuk::SamplerAddressMode::eRepeat;
    vuk::CompareOp compare_op = vuk::CompareOp::eNever;
    f32 max_anisotropy = 0.0f;
    f32 mip_lod_bias = 0.0f;
    f32 min_lod = 0.0f;
    f32 max_lod = 1000.0f;
    bool use_anisotropy = false;
};

struct Sampler {
    static auto create(Device &, const SamplerInfo &info, LR_THISCALL) -> std::expected<Sampler, vuk::VkException>;

    auto id() const -> SamplerID;
    auto index() const -> u32;

    explicit operator bool() const {
        return id_ != SamplerID::Invalid;
    }

private:
    SamplerID id_ = SamplerID::Invalid;
};

/////////////////////////////////
// PIPELINE

struct PipelineCompileInfo {
    // Root path is where included modules will be searched and linked
    std::string module_name = {};
    ls::option<std::string_view> shader_source = ls::nullopt;
    std::vector<std::string> entry_points = {};
};

struct Pipeline {
    static auto
    create(Device &, SlangSession &session, const PipelineCompileInfo &compile_info, ls::span<vuk::PersistentDescriptorSet> persistent_sets = {})
        -> std::expected<Pipeline, vuk::VkException>;

    auto id() const -> PipelineID;

private:
    PipelineID id_ = PipelineID::Invalid;
};

} // namespace lr
