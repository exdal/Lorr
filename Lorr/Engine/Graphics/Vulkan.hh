#pragma once

#include <vuk/Buffer.hpp>
#include <vuk/ImageAttachment.hpp>
#include <vuk/runtime/vk/Image.hpp>
#include <vuk/runtime/vk/Pipeline.hpp>
#include <vuk/runtime/vk/VkSwapchain.hpp>
#include <vuk/runtime/vk/VkTypes.hpp>

namespace lr {
enum class BufferID : u32 { Invalid = ~0_u32 };
enum class ImageID : u32 { Invalid = ~0_u32 };
enum class ImageViewID : u32 { Invalid = ~0_u32 };
enum class SamplerID : u32 { Invalid = ~0_u32 };
enum class PipelineID : u32 { Invalid = ~0_u32 };
enum class QueryTimestampPoolID : u32 { Invalid = ~0_u32 };

/////////////////////////////////
// DEVICE RESOURCES
enum class Descriptors : u32 {
    Samplers = 0,
    Images,
    StorageImages,
    StorageBuffers,
    BDA,
};

struct Device;

struct Buffer {
    static auto create(Device &, u64 size, vuk::MemoryUsage memory_usage = vuk::MemoryUsage::eGPUonly)
        -> std::expected<Buffer, vuk::VkException>;

    auto data_size() const -> u64;
    auto device_address() const -> u64;
    auto host_ptr() const -> u8 *;
    auto id() const -> BufferID;

    explicit operator bool() const { return id_ != BufferID::Invalid; }

private:
    u64 data_size_ = 0;
    void *host_data_ = nullptr;
    u64 device_address_ = 0;
    BufferID id_ = BufferID::Invalid;
};

struct Image {
    static auto create(
        Device &,
        vuk::Format format,
        const vuk::ImageUsageFlags &usage,
        vuk::ImageType type,
        vuk::Extent3D extent,
        u32 slice_count = 1,
        u32 mip_count = 1) -> std::expected<Image, vuk::VkException>;

    auto format() const -> vuk::Format;
    auto extent() const -> vuk::Extent3D;
    auto slice_count() const -> u32;
    auto mip_count() const -> u32;
    auto id() const -> ImageID;

    explicit operator bool() const { return id_ != ImageID::Invalid; }

private:
    vuk::Format format_ = vuk::Format::eUndefined;
    vuk::Extent3D extent_ = {};
    u32 slice_count_ = 1;
    u32 mip_levels_ = 1;
    ImageID id_ = ImageID::Invalid;
};

// Image views can have different formats depending on the creation flags
// Currently, we do not support it so view's format is automatically parent
// image format. See:
// https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkImageViewCreateInfo.html

struct ImageView {
    static auto create(
        Device &,
        Image &image,
        const vuk::ImageUsageFlags &image_usage,
        vuk::ImageViewType type,
        const vuk::ImageSubresourceRange &subresource_range) -> std::expected<ImageView, vuk::VkException>;

    auto get_attachment(Device &, vuk::ImageUsageFlagBits usage) const -> vuk::ImageAttachment;
    auto get_attachment(Device &, vuk::ImageAttachment::Preset preset) const -> vuk::ImageAttachment;

    auto format() const -> vuk::Format;
    auto extent() const -> vuk::Extent3D;
    auto subresource_range() const -> vuk::ImageSubresourceRange;
    auto image_id() const -> ImageID;
    auto id() const -> ImageViewID;

    explicit operator bool() const { return id_ != ImageViewID::Invalid; }

private:
    vuk::Format format_ = vuk::Format::eUndefined;
    vuk::Extent3D extent_ = {};
    vuk::ImageViewType type_ = vuk::ImageViewType::e2D;
    vuk::ImageSubresourceRange subresource_range_ = {};
    ImageID bound_image_id_ = ImageID::Invalid;
    ImageViewID id_ = ImageViewID::Invalid;
};

struct Sampler {
    static auto create(
        Device &,
        vuk::Filter min_filter,
        vuk::Filter mag_filter,
        vuk::SamplerMipmapMode mipmap_mode,
        vuk::SamplerAddressMode addr_u,
        vuk::SamplerAddressMode addr_v,
        vuk::SamplerAddressMode addr_w,
        vuk::CompareOp compare_op,
        f32 max_anisotropy = 0,
        f32 mip_lod_bias = 0,
        f32 min_lod = 0,
        f32 max_lod = 1000.0f,
        bool use_anisotropy = false) -> std::expected<Sampler, vuk::VkException>;

    auto id() const -> SamplerID;

    explicit operator bool() const { return id_ != SamplerID::Invalid; }

private:
    SamplerID id_ = SamplerID::Invalid;
};

struct SampledImage {
    ImageViewID image_view_id : 24;
    SamplerID sampler_id : 8;
    SampledImage() = default;
    SampledImage(ImageViewID image_view_id_, SamplerID sampler_id_)
        : image_view_id(image_view_id_),
          sampler_id(sampler_id_) {}
};

/////////////////////////////////
// PIPELINE

struct ShaderCompileInfo {
    std::vector<ls::pair<std::string, std::string>> macros = {};
    // Root path is where included modules will be searched and linked
    std::string module_name = {};
    fs::path root_path = {};
    fs::path shader_path = {};
    ls::option<std::string_view> shader_source = ls::nullopt;
    std::vector<std::string> entry_points = {};
};

struct Pipeline {
    static auto create(Device &, const ShaderCompileInfo &compile_info) -> std::expected<Pipeline, vuk::VkException>;

    auto id() const -> PipelineID;

private:
    PipelineID id_ = PipelineID::Invalid;
};

struct QueryTimestampPool {
    static auto create(Device &, u32 query_count) -> std::expected<QueryTimestampPool, vuk::VkException>;
    auto destroy() -> void;
    auto set_name(const std::string &) -> QueryTimestampPool &;

    auto get_results(u32 first_query, u32 count, ls::span<u64> time_stamps) const -> void;

    auto id() const -> QueryTimestampPoolID;

private:
    QueryTimestampPoolID id_ = QueryTimestampPoolID::Invalid;
};

/////////////////////////////////
// DEVICE

auto vulkan_get_os_surface(VkInstance instance, void *handle, PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr)
    -> std::expected<VkSurfaceKHR, VkResult>;

struct SwapChain;
struct Surface {
    static auto create(Device &, void *handle) -> std::expected<Surface, vuk::VkException>;

private:
    VkSurfaceKHR handle_ = VK_NULL_HANDLE;

    friend SwapChain;
};

struct SwapChain {
    static auto create(Device &, Surface &surface, vuk::Extent2D extent) -> std::expected<SwapChain, vuk::VkException>;

    auto format() const -> vuk::Format;
    auto extent() const -> vuk::Extent3D;

private:
    vuk::Format format_ = vuk::Format::eUndefined;
    vuk::Extent3D extent_ = {};

    Surface surface_ = {};
    ls::option<vuk::Swapchain> handle_;  // had to make option to make it stop cry

    friend Device;
};

}  // namespace lr
