#include "Engine/Graphics/Vulkan.hh"

#include "Engine/Graphics/VulkanDevice.hh"

#include "Engine/Memory/Stack.hh"

namespace lr {
auto Image::create(Device &device, const ImageInfo &info, LR_CALLSTACK) -> std::expected<Image, vuk::VkException> {
    ZoneScoped;

    vuk::ImageCreateInfo create_info = {
        .imageType = info.type,
        .format = info.format,
        .extent = info.extent,
        .mipLevels = info.mip_count,
        .arrayLayers = info.slice_count,
        .usage = info.usage | vuk::ImageUsageFlagBits::eTransferDst,
    };
    auto image_handle = vuk::Image{};
    auto result = device.allocator->allocate_images({ &image_handle, 1 }, { &create_info, 1 }, LOC);
    if (!result.holds_value()) {
        return std::unexpected(result.error());
    }

    auto image = Image{};
    image.format_ = info.format;
    image.extent_ = info.extent;
    image.slice_count_ = info.slice_count;
    image.mip_levels_ = info.mip_count;
    image.id_ = device.resources.images.create_slot(static_cast<vuk::Image &&>(image_handle));
    device.set_name(image, info.name);

    return image;
}

auto Image::create_with_view(Device &device, const ImageInfo &info, LR_CALLSTACK) -> std::expected<std::tuple<Image, ImageView>, vuk::VkException> {
    ZoneScoped;
    memory::ScopedStack stack;

    auto view_type = vuk::ImageViewType::eInfer;
    switch (info.type) {
        case vuk::ImageType::e1D:
            view_type = vuk::ImageViewType::e1D;
            break;
        case vuk::ImageType::e2D:
            view_type = vuk::ImageViewType::e2D;
            break;
        case vuk::ImageType::e3D:
            view_type = vuk::ImageViewType::e3D;
            break;
        case vuk::ImageType::eInfer:
            view_type = vuk::ImageViewType::eInfer;
            break;
    }

    auto aspect_mask = vuk::ImageAspectFlags{};
    switch (info.format) {
        case vuk::Format::eD32Sfloat:
        case vuk::Format::eD16Unorm:
        case vuk::Format::eD32SfloatS8Uint:
        case vuk::Format::eD24UnormS8Uint:
        case vuk::Format::eD16UnormS8Uint:
        case vuk::Format::eX8D24UnormPack32:
            aspect_mask = vuk::ImageAspectFlagBits::eDepth;
            break;
        default:
            aspect_mask = vuk::ImageAspectFlagBits::eColor;
            break;
    }

    auto subresource_range = vuk::ImageSubresourceRange{
        .aspectMask = aspect_mask,
        .baseMipLevel = 0,
        .levelCount = info.mip_count,
        .baseArrayLayer = 0,
        .layerCount = info.slice_count,
    };
    auto image = Image::create(device, info, LOC).value();

    auto view_name = !info.name.empty() ? stack.format("{} View", info.name) : "";
    auto image_view_info = ImageViewInfo{
        .image_usage = info.usage,
        .type = view_type,
        .subresource_range = subresource_range,
        .name = view_name,
    };
    auto image_view = ImageView::create(device, image, image_view_info, LOC).value();

    return std::make_tuple(image, image_view);
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

auto ImageView::create(Device &device, Image &image, const ImageViewInfo &info, LR_CALLSTACK) -> std::expected<ImageView, vuk::VkException> {
    ZoneScoped;

    auto image_handle = device.image(image.id()).value_or(vuk::Image{});
    vuk::ImageViewCreateInfo create_info = {
        .image = image_handle.image,
        .viewType = info.type,
        .format = image.format(),
        .components = {
            .r = vuk::ComponentSwizzle::eIdentity,
            .g = vuk::ComponentSwizzle::eIdentity,
            .b = vuk::ComponentSwizzle::eIdentity,
            .a = vuk::ComponentSwizzle::eIdentity,
        },
        .subresourceRange = info.subresource_range,
        .view_usage = info.image_usage,
    };
    auto image_view_handle = vuk::ImageView{};
    auto result = device.allocator->allocate_image_views({ &image_view_handle, 1 }, { &create_info, 1 }, LOC);
    if (!result.holds_value()) {
        return std::unexpected(result.error());
    }

    auto image_view = ImageView{};
    image_view.format_ = image.format();
    image_view.extent_ = image.extent();
    image_view.type_ = info.type;
    image_view.subresource_range_ = info.subresource_range;
    image_view.bound_image_id_ = image.id();
    image_view.id_ = device.resources.image_views.create_slot(static_cast<vuk::ImageView &&>(image_view_handle));
    device.set_name(image_view, info.name);

    return image_view;
}

auto ImageView::to_attachment(Device &device, const vuk::ImageUsageFlags &usage) const -> vuk::ImageAttachment {
    ZoneScoped;

    auto image_handle = device.image(bound_image_id_);
    auto view_handle = device.image_view(id_);

    return vuk::ImageAttachment{
        .image = image_handle.value_or(vuk::Image{}),
        .image_view = view_handle.value_or(vuk::ImageView{}),
        .usage = usage,
        .extent = extent_,
        .format = format_,
        .sample_count = vuk::Samples::e1,
        .view_type = type_,
        .components = {},
        .base_level = 0,
        .level_count = this->mip_count(),
        .base_layer = 0,
        .layer_count = this->slice_count(),
    };
}

auto ImageView::to_attachment(Device &device, vuk::ImageAttachment::Preset preset) const -> vuk::ImageAttachment {
    ZoneScoped;

    auto image_handle = device.image(bound_image_id_);
    auto view_handle = device.image_view(id_);
    auto attachment = vuk::ImageAttachment::from_preset(preset, format_, extent_, vuk::Samples::e1);
    attachment.image = image_handle.value_or(vuk::Image{});
    attachment.image_view = view_handle.value_or(vuk::ImageView{});

    return attachment;
}

auto ImageView::discard(Device &device, vuk::Name name, const vuk::ImageUsageFlags &usage, LR_CALLSTACK) const -> vuk::Value<vuk::ImageAttachment> {
    ZoneScoped;

    return vuk::discard_ia(name, this->to_attachment(device, usage), LOC);
}

auto ImageView::acquire(Device &device, vuk::Name name, const vuk::ImageUsageFlags &usage, vuk::Access last_access, LR_CALLSTACK) const
    -> vuk::Value<vuk::ImageAttachment> {
    ZoneScoped;

    return vuk::acquire_ia(name, this->to_attachment(device, usage), last_access, LOC);
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

auto ImageView::index() const -> u32 {
    return SlotMap_decode_id(id_).index;
}

auto Sampler::create(Device &device, const SamplerInfo &info, [[maybe_unused]] vuk::source_location LOC) -> std::expected<Sampler, vuk::VkException> {
    ZoneScoped;

    vuk::SamplerCreateInfo create_info = {
        .magFilter = info.mag_filter,
        .minFilter = info.min_filter,
        .mipmapMode = info.mipmap_mode,
        .addressModeU = info.addr_u,
        .addressModeV = info.addr_v,
        .addressModeW = info.addr_w,
        .mipLodBias = info.mip_lod_bias,
        .anisotropyEnable = info.use_anisotropy,
        .maxAnisotropy = info.max_anisotropy,
        .compareEnable = info.compare_op != vuk::CompareOp::eNever,
        .compareOp = info.compare_op,
        .minLod = info.min_lod,
        .maxLod = info.max_lod,
    };

    auto sampler = Sampler{};
    sampler.id_ = device.resources.samplers.create_slot();
    auto *sampler_handle = device.resources.samplers.slot(sampler.id_);
    *sampler_handle = device.runtime->acquire_sampler(create_info, device.frame_count());

    return sampler;
}

auto Sampler::id() const -> SamplerID {
    return id_;
}

auto Sampler::index() const -> u32 {
    return SlotMap_decode_id(id_).index;
}

} // namespace lr
