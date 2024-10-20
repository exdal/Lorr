#include "Engine/Graphics/Vulkan/Impl.hh"

#include "Engine/Graphics/Vulkan/ToVk.hh"

namespace lr {
auto Image::create(
    Device_H device,
    vk::ImageUsage usage_flags,
    vk::Format format,
    vk::ImageType type,
    vk::ImageAspectFlag aspect_flags,
    vk::Extent3D extent,
    u32 slice_count,
    u32 mip_levels,
    const std::string &name) -> std::expected<ImageID, vk::Result> {
    ZoneScoped;

    LS_EXPECT(!extent.is_zero());

    VkImageCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .imageType = to_vk_image_type(type),
        .format = get_format_info(format).vk_format,
        .extent = { extent.width, extent.height, extent.depth },
        .mipLevels = mip_levels,
        .arrayLayers = slice_count,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = to_vk_image_usage(usage_flags),
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    VmaAllocationCreateInfo allocation_info = {
        .flags = 0,
        .usage = VMA_MEMORY_USAGE_GPU_ONLY,
        .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        .preferredFlags = 0,
        .memoryTypeBits = 0,
        .pool = nullptr,
        .pUserData = nullptr,
        .priority = 0.5f,
    };

    VkImage image_handle = {};
    VmaAllocation allocation = {};
    VmaAllocationInfo allocation_result = {};
    auto result = vmaCreateImage(device->allocator, &create_info, &allocation_info, &image_handle, &allocation, &allocation_result);
    if (result != VK_SUCCESS) {
        return std::unexpected(vk::Result::Unknown);
    }

#if LS_DEBUG
    vmaSetAllocationName(device->allocator, allocation, name.c_str());
#endif
    set_object_name(device, image_handle, VK_OBJECT_TYPE_IMAGE, name);

    auto image_id = device->resources.images.create();
    if (!image_id.has_value()) {
        return std::unexpected(vk::Result::OutOfPoolMem);
    }

    auto impl = image_id->impl;
    impl->device = device;
    impl->usage_flags = usage_flags;
    impl->format = format;
    impl->aspect_flags = aspect_flags;
    impl->extent = extent;
    impl->slice_count = slice_count;
    impl->mip_levels = mip_levels;
    impl->allocation = allocation;
    impl->handle = image_handle;

    return image_id->id;
}

auto Image::destroy() -> void {
    vmaDestroyImage(impl->device->allocator, impl->handle, impl->allocation);
}

auto Image::format() -> vk::Format {
    return impl->format;
}

auto Image::extent() -> vk::Extent3D {
    return impl->extent;
}

auto Image::subresource_range() -> vk::ImageSubresourceRange {
    return {
        .aspect_flags = impl->aspect_flags,
        .base_mip = 0,
        .mip_count = impl->mip_levels,
        .base_slice = 0,
        .slice_count = impl->slice_count,
    };
}

auto ImageView::create(
    Device_H device, ImageID image_id, vk::ImageViewType type, vk::ImageSubresourceRange subresource_range, const std::string &name)
    -> std::expected<ImageViewID, vk::Result> {
    ZoneScoped;

    auto &image = device->resources.images.get(image_id);

    VkImageViewCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .image = image.handle,
        .viewType = to_vk_image_view_type(type),
        .format = get_format_info(image.format).vk_format,
        .components = { VK_COMPONENT_SWIZZLE_IDENTITY,
                        VK_COMPONENT_SWIZZLE_IDENTITY,
                        VK_COMPONENT_SWIZZLE_IDENTITY,
                        VK_COMPONENT_SWIZZLE_IDENTITY },
        .subresourceRange = {
            .aspectMask = to_vk_image_aspect_flags(subresource_range.aspect_flags),
            .baseMipLevel = subresource_range.base_mip,
            .levelCount = subresource_range.mip_count,
            .baseArrayLayer = subresource_range.base_slice,
            .layerCount = subresource_range.slice_count,
        },
    };

    VkImageView image_view_handle = {};
    auto result = vkCreateImageView(device->handle, &create_info, nullptr, &image_view_handle);
    switch (result) {
        case VK_ERROR_OUT_OF_HOST_MEMORY:
            return std::unexpected(vk::Result::OutOfHostMem);
        case VK_ERROR_OUT_OF_DEVICE_MEMORY:
            return std::unexpected(vk::Result::OutOfDeviceMem);
        default:;
    }

    set_object_name(device, image_view_handle, VK_OBJECT_TYPE_SAMPLER, name);

    auto image_view = device->resources.image_views.create();
    if (!image_view.has_value()) {
        return std::unexpected(vk::Result::OutOfPoolMem);
    }

    auto impl = image_view->impl;
    impl->device = device;
    impl->format = image.format;
    impl->type = type;
    impl->subresource_range = subresource_range;
    impl->handle = image_view_handle;

    ls::static_vector<VkWriteDescriptorSet, 2> descriptor_writes = {};
    VkDescriptorImageInfo sampled_descriptor_info = {
        .imageView = impl->handle,
        .imageLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
        .sampler = VK_NULL_HANDLE,
    };
    VkDescriptorImageInfo storage_descriptor_info = {
        .imageView = impl->handle,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
        .sampler = VK_NULL_HANDLE,
    };

    VkWriteDescriptorSet sampled_write_set_info = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,
        .dstSet = device->descriptor_set,
        .dstBinding = std::to_underlying(Descriptors::Images),
        .dstArrayElement = std::to_underlying(image_view->id),
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        .pImageInfo = &sampled_descriptor_info,
        .pBufferInfo = nullptr,
        .pTexelBufferView = nullptr,
    };
    VkWriteDescriptorSet storage_write_set_info = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,
        .dstSet = device->descriptor_set,
        .dstBinding = std::to_underlying(Descriptors::StorageImages),
        .dstArrayElement = std::to_underlying(image_view->id),
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        .pImageInfo = &storage_descriptor_info,
        .pBufferInfo = nullptr,
        .pTexelBufferView = nullptr,
    };
    if (image.usage_flags & vk::ImageUsage::Sampled) {
        descriptor_writes.push_back(sampled_write_set_info);
    }
    if (image.usage_flags & vk::ImageUsage::Storage) {
        descriptor_writes.push_back(storage_write_set_info);
    }

    if (!descriptor_writes.empty()) {
        vkUpdateDescriptorSets(device->handle, descriptor_writes.size(), descriptor_writes.data(), 0, nullptr);
    }

    return image_view->id;
}

auto ImageView::destroy() -> void {
    vkDestroyImageView(impl->device->handle, impl->handle, nullptr);
}

auto ImageView::format() -> vk::Format {
    return impl->format;
}

auto ImageView::aspect_flags() -> vk::ImageAspectFlag {
    return impl->subresource_range.aspect_flags;
}

auto ImageView::subresource_range() -> vk::ImageSubresourceRange {
    return impl->subresource_range;
}

auto Sampler::create(Device_H device, const SamplerInfo &info, const std::string &name) -> std::expected<SamplerID, vk::Result> {
    ZoneScoped;

    VkSamplerCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .magFilter = to_vk_filtering(info.mag_filter),
        .minFilter = to_vk_filtering(info.min_filter),
        .mipmapMode = to_vk_mipmap_address_mode(info.mip_filter),
        .addressModeU = to_vk_address_mode(info.address_u),
        .addressModeV = to_vk_address_mode(info.address_v),
        .addressModeW = to_vk_address_mode(info.address_w),
        .mipLodBias = info.mip_lod_bias,
        .anisotropyEnable = info.use_anisotropy,
        .maxAnisotropy = info.max_anisotropy,
        .compareEnable = info.compare_op != vk::CompareOp::Never,
        .compareOp = to_vk_compare_op(info.compare_op),
        .minLod = info.min_lod,
        .maxLod = info.max_lod,
        .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK,
        .unnormalizedCoordinates = 0,
    };

    VkSampler sampler_handle = {};
    auto result = vkCreateSampler(device->handle, &create_info, nullptr, &sampler_handle);
    switch (result) {
        case VK_ERROR_OUT_OF_HOST_MEMORY:
            return std::unexpected(vk::Result::OutOfHostMem);
        case VK_ERROR_OUT_OF_DEVICE_MEMORY:
            return std::unexpected(vk::Result::OutOfDeviceMem);
        default:;
    }

    set_object_name(device, sampler_handle, VK_OBJECT_TYPE_SAMPLER, name);

    auto sampler = device->resources.samplers.create();
    if (!sampler.has_value()) {
        return std::unexpected(vk::Result::OutOfPoolMem);
    }

    auto impl = sampler->impl;
    impl->device = device;
    impl->handle = sampler_handle;

    VkDescriptorImageInfo sampler_descriptor_info = {
        .sampler = impl->handle,
        .imageView = VK_NULL_HANDLE,
        .imageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    VkWriteDescriptorSet sampler_write_set_info = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,
        .dstSet = device->descriptor_set,
        .dstBinding = std::to_underlying(Descriptors::Images),
        .dstArrayElement = std::to_underlying(sampler->id),
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
        .pImageInfo = &sampler_descriptor_info,
        .pBufferInfo = nullptr,
        .pTexelBufferView = nullptr,
    };

    vkUpdateDescriptorSets(device->handle, 1, &sampler_write_set_info, 0, nullptr);

    return sampler->id;
}

auto Sampler::destroy() -> void {
    vkDestroySampler(impl->device->handle, impl->handle, nullptr);
}

}  // namespace lr
