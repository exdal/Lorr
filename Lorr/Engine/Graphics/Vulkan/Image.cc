#include "Engine/Graphics/Vulkan.hh"

#include "Engine/Graphics/VulkanDevice.hh"

namespace lr {
auto Image::create(
    Device &device,
    vuk::Format format,
    const vuk::ImageUsageFlags &usage,
    vuk::ImageType type,
    vuk::Extent3D extent,
    u32 slice_count,
    u32 mip_count,
    vuk::source_location LOC
) -> std::expected<Image, vuk::VkException> {
    ZoneScoped;

    vuk::ImageCreateInfo create_info = {
        .imageType = type,
        .format = format,
        .extent = extent,
        .mipLevels = mip_count,
        .arrayLayers = slice_count,
        .usage = usage | vuk::ImageUsageFlagBits::eTransferDst,
    };
    vuk::Unique<vuk::Image> image_handle(*device.allocator);
    auto result = device.allocator->allocate_images({ &*image_handle, 1 }, { &create_info, 1 }, LOC);
    if (!result.holds_value()) {
        return std::unexpected(result.error());
    }

    auto image = Image {};
    image.format_ = format;
    image.extent_ = extent;
    image.slice_count_ = slice_count;
    image.mip_levels_ = mip_count;
    image.id_ = device.resources.images.create_slot(std::move(image_handle));
    return image;
}

auto Image::format() const -> vuk::Format {
    return format_;
}

auto Image::extent() const -> vuk::Extent3D {
    return extent_;
}

auto Image::slice_count() const -> u32 {
    return slice_count_;
}

auto Image::mip_count() const -> u32 {
    return mip_levels_;
}

auto Image::id() const -> ImageID {
    return id_;
}

auto ImageView::create(
    Device &device,
    Image &image,
    const vuk::ImageUsageFlags &image_usage,
    vuk::ImageViewType type,
    const vuk::ImageSubresourceRange &subresource_range,
    vuk::source_location LOC
) -> std::expected<ImageView, vuk::VkException> {
    ZoneScoped;

    auto image_handle = device.image(image.id());
    vuk::ImageViewCreateInfo create_info = {
        .image = image_handle->image,
        .viewType = type,
        .format = image.format(),
        .components = {
            .r = vuk::ComponentSwizzle::eIdentity,
            .g = vuk::ComponentSwizzle::eIdentity,
            .b = vuk::ComponentSwizzle::eIdentity,
            .a = vuk::ComponentSwizzle::eIdentity,
        },
        .subresourceRange = subresource_range,
        .view_usage = image_usage,
    };
    vuk::Unique<vuk::ImageView> image_view_handle(*device.allocator);
    auto result = device.allocator->allocate_image_views({ &*image_view_handle, 1 }, { &create_info, 1 }, LOC);
    if (!result.holds_value()) {
        return std::unexpected(result.error());
    }

    auto image_view = ImageView {};
    image_view.format_ = image.format();
    image_view.extent_ = image.extent();
    image_view.type_ = type;
    image_view.subresource_range_ = subresource_range;
    image_view.bound_image_id_ = image.id();
    image_view.id_ = device.resources.image_views.create_slot(std::move(image_view_handle));

    return image_view;
}

auto ImageView::get_attachment(Device &device, const vuk::ImageUsageFlags &usage) const -> vuk::ImageAttachment {
    ZoneScoped;

    auto *image_handle = device.image(bound_image_id_);
    auto *view_handle = device.image_view(id_);

    return vuk::ImageAttachment {
        .image = *image_handle,
        .image_view = *view_handle,
        .usage = usage,
        .extent = extent_,
        .format = format_,
        .sample_count = vuk::Samples::e1,
        .components = {},
        .base_level = 0,
        .level_count = this->mip_count(),
        .base_layer = 0,
        .layer_count = this->slice_count(),
    };
}

auto ImageView::get_attachment(Device &device, vuk::ImageAttachment::Preset preset) const -> vuk::ImageAttachment {
    ZoneScoped;

    auto *image_handle = device.image(bound_image_id_);
    auto *view_handle = device.image_view(id_);
    auto attachment = vuk::ImageAttachment::from_preset(preset, format_, extent_, vuk::Samples::e1);
    attachment.image = *image_handle;
    attachment.image_view = *view_handle;

    return attachment;
}

auto ImageView::discard(Device &device, vuk::Name name, const vuk::ImageUsageFlags &usage) const -> vuk::Value<vuk::ImageAttachment> {
    ZoneScoped;

    return vuk::discard_ia(name, this->get_attachment(device, usage));
}

auto ImageView::acquire(Device &device, vuk::Name name, const vuk::ImageUsageFlags &usage, vuk::Access last_access) const
    -> vuk::Value<vuk::ImageAttachment> {
    ZoneScoped;

    return vuk::acquire_ia(name, this->get_attachment(device, usage), last_access);
}

auto ImageView::format() const -> vuk::Format {
    return format_;
}

auto ImageView::extent() const -> vuk::Extent3D {
    return extent_;
}

auto ImageView::subresource_range() const -> vuk::ImageSubresourceRange {
    return subresource_range_;
}

auto ImageView::slice_count() const -> u32 {
    return subresource_range_.layerCount;
}

auto ImageView::mip_count() const -> u32 {
    return subresource_range_.levelCount;
}

auto ImageView::image_id() const -> ImageID {
    return bound_image_id_;
}

auto ImageView::id() const -> ImageViewID {
    return id_;
}

auto Sampler::create(
    Device &device,
    vuk::Filter min_filter,
    vuk::Filter mag_filter,
    vuk::SamplerMipmapMode mipmap_mode,
    vuk::SamplerAddressMode addr_u,
    vuk::SamplerAddressMode addr_v,
    vuk::SamplerAddressMode addr_w,
    vuk::CompareOp compare_op,
    f32 max_anisotropy,
    f32 mip_lod_bias,
    f32 min_lod,
    f32 max_lod,
    bool use_anisotropy,
    [[maybe_unused]] vuk::source_location LOC
) -> std::expected<Sampler, vuk::VkException> {
    ZoneScoped;

    vuk::SamplerCreateInfo create_info = {
        .magFilter = mag_filter,
        .minFilter = min_filter,
        .mipmapMode = mipmap_mode,
        .addressModeU = addr_u,
        .addressModeV = addr_v,
        .addressModeW = addr_w,
        .mipLodBias = mip_lod_bias,
        .anisotropyEnable = use_anisotropy,
        .maxAnisotropy = max_anisotropy,
        .compareEnable = compare_op != vuk::CompareOp::eNever,
        .compareOp = compare_op,
        .minLod = min_lod,
        .maxLod = max_lod,
    };

    auto sampler = Sampler {};
    sampler.id_ = device.resources.samplers.create_slot();
    auto *sampler_handle = device.resources.samplers.slot(sampler.id_);
    *sampler_handle = device.runtime->acquire_sampler(create_info, device.frame_count());

    return sampler;
}

auto Sampler::id() const -> SamplerID {
    return id_;
}

} // namespace lr
