#include "Engine/Graphics/Vulkan/Impl.hh"

#include "Engine/Graphics/Vulkan/ToVk.hh"
#include "Engine/Memory/Hasher.hh"

namespace lr {
auto Image::create(
    Device device,
    vk::ImageUsage usage,
    vk::Format format,
    vk::ImageType type,
    vk::Extent3D extent,
    vk::ImageAspectFlag aspect_flags,
    u32 slice_count,
    u32 mip_count) -> std::expected<Image, vk::Result> {
    ZoneScoped;

    LS_EXPECT(!extent.is_zero());

    VkImageCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .imageType = to_vk_image_type(type),
        .format = get_format_info(format).vk_format,
        .extent = { extent.width, extent.height, extent.depth },
        .mipLevels = mip_count,
        .arrayLayers = slice_count,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = to_vk_image_usage(usage),
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    VmaAllocationCreateInfo allocation_info = {
        .flags = 0,
        .usage = VMA_MEMORY_USAGE_AUTO,
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

    auto image = device->resources.images.create();
    if (!image.has_value()) {
        return std::unexpected(vk::Result::OutOfPoolMem);
    }

    vk::ImageViewType view_type = {};
    switch (type) {
        case vk::ImageType::View1D:
            view_type = vk::ImageViewType::View1D;
            break;
        case vk::ImageType::View2D:
            view_type = vk::ImageViewType::View2D;
            break;
        case vk::ImageType::View3D:
            view_type = vk::ImageViewType::View3D;
            break;
    }

    auto impl = image->self;
    auto self = Image(impl);
    impl->device = device;
    impl->usage_flags = usage;
    impl->format = format;
    impl->aspect_flags = aspect_flags;
    impl->extent = extent;
    impl->slice_count = slice_count;
    impl->mip_levels = mip_count;
    impl->id = image->id;
    impl->allocation = allocation;
    impl->handle = image_handle;

    // The reason why this exist is because i am fucking lazy, okay?
    impl->default_view = ImageView::create(device, self, view_type, self.subresource_range()).value();

    return self;
}

auto Image::create_for_swap_chain(Device_H device, vk::Format format, vk::Extent3D extent) -> std::expected<Image, vk::Result> {
    ZoneScoped;

    auto image = device->resources.images.create();
    if (!image.has_value()) {
        return std::unexpected(vk::Result::OutOfPoolMem);
    }

    auto impl = image->self;
    impl->device = device;
    impl->usage_flags = vk::ImageUsage::ColorAttachment;
    impl->format = format;
    impl->aspect_flags = vk::ImageAspectFlag::Color;
    impl->extent = extent;
    impl->slice_count = 1;
    impl->mip_levels = 1;
    impl->id = image->id;
    impl->allocation = nullptr;
    impl->handle = nullptr;

    return Image(impl);
}

auto Image::destroy() -> void {
    impl->default_view.destroy();
    vmaDestroyImage(impl->device->allocator, impl->handle, impl->allocation);
    impl->device->resources.images.destroy(impl->id);
}

auto Image::set_name(const std::string &name) -> Image & {
    if (name.empty()) {
        return *this;
    }

    if (impl->allocation) {
        vmaSetAllocationName(impl->device->allocator, impl->allocation, name.c_str());
    }
    set_object_name(impl->device, impl->handle, VK_OBJECT_TYPE_IMAGE, name);
    impl->default_view.set_name(name + " View");

    return *this;
}

auto Image::generate_mips(vk::ImageLayout old_layout) -> Image & {
    ZoneScoped;

    LS_EXPECT(impl->aspect_flags & vk::ImageAspectFlag::Color);
    auto graphics_queue = impl->device.queue(vk::CommandType::Graphics);
    auto image_cmd_list = graphics_queue.begin();

    image_cmd_list.image_transition({
        .src_stage = vk::PipelineStage::AllCommands,
        .src_access = vk::MemoryAccess::ReadWrite,
        .dst_stage = vk::PipelineStage::Blit,
        .dst_access = vk::MemoryAccess::TransferRead,
        .old_layout = old_layout,
        .new_layout = vk::ImageLayout::TransferSrc,
        .image_id = impl->id,
        .subresource_range = { impl->aspect_flags, 0, 1, 0, 1 },
    });

    i32 width = static_cast<i32>(impl->extent.width);
    i32 height = static_cast<i32>(impl->extent.height);
    for (u32 i = 1; i < impl->mip_levels; i++) {
        image_cmd_list.image_transition({
            .dst_stage = vk::PipelineStage::Blit,
            .dst_access = vk::MemoryAccess::TransferWrite,
            .new_layout = vk::ImageLayout::TransferDst,
            .image_id = impl->id,
            .subresource_range = { vk::ImageAspectFlag::Color, i, 1, 0, 1 },
        });

        vk::ImageBlit blit = {
            .src_subresource = { vk::ImageAspectFlag::Color, i - 1, 0, 1 },
            .src_offsets = { {}, { width >> (i - 1), height >> (i - 1), 1 } },
            .dst_subresource = { vk::ImageAspectFlag::Color, i, 0, 1 },
            .dst_offsets = { {}, { width >> i, height >> i, 1 } },
        };
        image_cmd_list.blit_image(
            impl->id, vk::ImageLayout::TransferSrc, impl->id, vk::ImageLayout::TransferDst, vk::Filtering::Linear, blit);

        image_cmd_list.image_transition({
            .src_stage = vk::PipelineStage::Blit,
            .src_access = vk::MemoryAccess::TransferWrite,
            .dst_stage = vk::PipelineStage::Blit,
            .dst_access = vk::MemoryAccess::TransferRead,
            .old_layout = vk::ImageLayout::TransferDst,
            .new_layout = vk::ImageLayout::TransferSrc,
            .image_id = impl->id,
            .subresource_range = { vk::ImageAspectFlag::Color, i, 1, 0, 1 },
        });
    }

    image_cmd_list.image_transition({
        .src_stage = vk::PipelineStage::Blit,
        .src_access = vk::MemoryAccess::TransferRead,
        .dst_stage = vk::PipelineStage::AllCommands,
        .dst_access = vk::MemoryAccess::ReadWrite,
        .old_layout = vk::ImageLayout::TransferSrc,
        .new_layout = old_layout,
        .image_id = impl->id,
        .subresource_range = { vk::ImageAspectFlag::Color, 0, impl->mip_levels, 0, 1 },
    });

    graphics_queue.end(image_cmd_list);
    graphics_queue.submit({}, {});
    graphics_queue.wait();

    return *this;
}

auto Image::set_data(void *data, u64 data_size, vk::ImageLayout new_layout) -> Image & {
    ZoneScoped;

    LS_EXPECT(data_size <= Device::Limits::PersistentBufferSize);
    auto graphics_queue = impl->device.queue(vk::CommandType::Graphics);
    auto cmd_list = graphics_queue.begin();
    auto transfer_man = impl->device.transfer_man();

    const auto &format_info = get_format_info(impl->format);
    auto rows = (impl->extent.height + format_info.block_height - 1) / format_info.block_height;
    auto cols = (impl->extent.width + format_info.block_width - 1) / format_info.block_width;
    auto pixel_size = rows * cols * format_info.component_size;
    LS_EXPECT(pixel_size == data_size);

    auto allocation = transfer_man.allocate(pixel_size);
    std::memcpy(allocation->ptr, data, data_size);

    cmd_list.image_transition({
        .src_stage = vk::PipelineStage::TopOfPipe,
        .src_access = vk::MemoryAccess::None,
        .dst_stage = vk::PipelineStage::Copy,
        .dst_access = vk::MemoryAccess::TransferWrite,
        .old_layout = vk::ImageLayout::Undefined,
        .new_layout = vk::ImageLayout::TransferDst,
        .image_id = impl->id,
    });
    vk::ImageCopyRegion copy_region = {
        .buffer_offset = allocation->offset,
        .image_extent = impl->extent,
    };
    cmd_list.copy_buffer_to_image(allocation->cpu_buffer_id, impl->id, vk::ImageLayout::TransferDst, copy_region);
    cmd_list.image_transition({
        .src_stage = vk::PipelineStage::Copy,
        .src_access = vk::MemoryAccess::TransferWrite,
        .dst_stage = vk::PipelineStage::AllCommands,
        .dst_access = vk::MemoryAccess::ReadWrite,
        .old_layout = vk::ImageLayout::TransferDst,
        .new_layout = new_layout,
        .image_id = impl->id,
    });

    graphics_queue.end(cmd_list);
    graphics_queue.submit({}, transfer_man.semaphore());
    graphics_queue.wait();
    transfer_man.collect_garbage();

    return *this;
}

auto Image::transition(vk::ImageLayout from, vk::ImageLayout to) -> Image & {
    ZoneScoped;

    auto queue = impl->device.queue(vk::CommandType::Graphics);
    auto cmd_list = queue.begin();
    cmd_list.image_transition({
        .src_stage = vk::PipelineStage::AllCommands,
        .src_access = vk::MemoryAccess::ReadWrite,
        .dst_stage = vk::PipelineStage::AllCommands,
        .dst_access = vk::MemoryAccess::ReadWrite,
        .old_layout = from,
        .new_layout = to,
        .image_id = impl->id,
        .subresource_range = this->subresource_range(),
    });
    queue.end(cmd_list);
    queue.submit({}, {});
    queue.wait();

    return *this;
}

auto Image::format() const -> vk::Format {
    return impl->format;
}

auto Image::extent() const -> vk::Extent3D {
    return impl->extent;
}

auto Image::slice_count() const -> u32 {
    return impl->slice_count;
}

auto Image::mip_count() const -> u32 {
    return impl->mip_levels;
}

auto Image::subresource_range() const -> vk::ImageSubresourceRange {
    return {
        .aspect_flags = impl->aspect_flags,
        .base_mip = 0,
        .mip_count = impl->mip_levels,
        .base_slice = 0,
        .slice_count = impl->slice_count,
    };
}

auto Image::view() const -> ImageView {
    return impl->default_view;
}

auto Image::id() const -> ImageID {
    return impl->id;
}

auto ImageView::create(Device_H device, Image image, vk::ImageViewType type, vk::ImageSubresourceRange subresource_range)
    -> std::expected<ImageView, vk::Result> {
    ZoneScoped;

    VkImageViewCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .image = image->handle,
        .viewType = to_vk_image_view_type(type),
        .format = get_format_info(image.format()).vk_format,
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

    auto image_view = device->resources.image_views.create();
    if (!image_view.has_value()) {
        return std::unexpected(vk::Result::OutOfPoolMem);
    }

    auto impl = image_view->self;
    impl->device = device;
    impl->format = image.format();
    impl->type = type;
    impl->subresource_range = subresource_range;
    impl->id = image_view->id;
    impl->handle = image_view_handle;

    ls::static_vector<VkWriteDescriptorSet, 2> descriptor_writes = {};
    VkDescriptorImageInfo sampled_descriptor_info = {
        .sampler = VK_NULL_HANDLE,
        .imageView = impl->handle,
        .imageLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
    };
    VkDescriptorImageInfo storage_descriptor_info = {
        .sampler = VK_NULL_HANDLE,
        .imageView = impl->handle,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
    };

    VkWriteDescriptorSet sampled_write_set_info = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,
        .dstSet = impl->device->descriptor_set,
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
        .dstSet = impl->device->descriptor_set,
        .dstBinding = std::to_underlying(Descriptors::StorageImages),
        .dstArrayElement = std::to_underlying(image_view->id),
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        .pImageInfo = &storage_descriptor_info,
        .pBufferInfo = nullptr,
        .pTexelBufferView = nullptr,
    };
    if (image->usage_flags & vk::ImageUsage::Sampled) {
        descriptor_writes.push_back(sampled_write_set_info);
    }
    if (image->usage_flags & vk::ImageUsage::Storage) {
        descriptor_writes.push_back(storage_write_set_info);
    }

    if (!descriptor_writes.empty()) {
        vkUpdateDescriptorSets(impl->device->handle, descriptor_writes.size(), descriptor_writes.data(), 0, nullptr);
    }

    return ImageView(impl);
}

auto ImageView::destroy() -> void {
    vkDestroyImageView(impl->device->handle, impl->handle, nullptr);
    impl->device->resources.image_views.destroy(impl->id);
}

auto ImageView::set_name(const std::string &name) -> ImageView & {
    set_object_name(impl->device, impl->handle, VK_OBJECT_TYPE_IMAGE_VIEW, name);

    return *this;
}

auto ImageView::format() const -> vk::Format {
    return impl->format;
}

auto ImageView::aspect_flags() const -> vk::ImageAspectFlag {
    return impl->subresource_range.aspect_flags;
}

auto ImageView::subresource_range() const -> vk::ImageSubresourceRange {
    return impl->subresource_range;
}

auto ImageView::id() const -> ImageViewID {
    return impl->id;
}

struct CachedSamplerInfo {
    vk::Filtering min_filter = {};
    vk::Filtering mag_filter = {};
    vk::Filtering mip_filter = {};
    vk::SamplerAddressMode addr_u = {};
    vk::SamplerAddressMode addr_v = {};
    vk::SamplerAddressMode addr_w = {};
    vk::CompareOp compare_op = {};
    f32 max_anisotropy = {};
    f32 mip_lod_bias = {};
    f32 min_lod = {};
    f32 max_lod = {};
};

auto Sampler::create(
    Device device,
    vk::Filtering min_filter,
    vk::Filtering mag_filter,
    vk::Filtering mip_filter,
    vk::SamplerAddressMode addr_u,
    vk::SamplerAddressMode addr_v,
    vk::SamplerAddressMode addr_w,
    vk::CompareOp compare_op,
    f32 max_anisotropy,
    f32 mip_lod_bias,
    f32 min_lod,
    f32 max_lod,
    bool use_anisotropy) -> std::expected<Sampler, vk::Result> {
    ZoneScoped;

    CachedSamplerInfo cached_sampler_info = {
        .min_filter = min_filter,
        .mag_filter = mag_filter,
        .mip_filter = mip_filter,
        .addr_u = addr_u,
        .addr_v = addr_v,
        .addr_w = addr_w,
        .compare_op = compare_op,
        .max_anisotropy = max_anisotropy,
        .mip_lod_bias = mip_lod_bias,
        .min_lod = min_lod,
        .max_lod = max_lod,
    };

    HasherXXH64 sampler_hasher = {};
    sampler_hasher.hash(&cached_sampler_info, sizeof(CachedSamplerInfo));
    auto hash_val = sampler_hasher.value();

    // its not worth caching a sampler with variable anisotropy
    if (!use_anisotropy) {
        for (auto &[avail_hash, sampler_id] : device->resources.cached_samplers) {
            if (avail_hash == hash_val) {
                return device.sampler(sampler_id);
            }
        }
    }

    VkSamplerCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .magFilter = to_vk_filtering(mag_filter),
        .minFilter = to_vk_filtering(min_filter),
        .mipmapMode = to_vk_mipmap_address_mode(mip_filter),
        .addressModeU = to_vk_address_mode(addr_u),
        .addressModeV = to_vk_address_mode(addr_v),
        .addressModeW = to_vk_address_mode(addr_w),
        .mipLodBias = mip_lod_bias,
        .anisotropyEnable = use_anisotropy,
        .maxAnisotropy = max_anisotropy,
        .compareEnable = compare_op != vk::CompareOp::Never,
        .compareOp = to_vk_compare_op(compare_op),
        .minLod = min_lod,
        .maxLod = max_lod,
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

    auto sampler = device->resources.samplers.create();
    if (!sampler.has_value()) {
        return std::unexpected(vk::Result::OutOfPoolMem);
    }

    device->resources.cached_samplers.emplace_back(hash_val, sampler->id);

    auto impl = sampler->self;
    impl->device = device;
    impl->id = sampler->id;
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
        .dstBinding = std::to_underlying(Descriptors::Samplers),
        .dstArrayElement = std::to_underlying(sampler->id),
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
        .pImageInfo = &sampler_descriptor_info,
        .pBufferInfo = nullptr,
        .pTexelBufferView = nullptr,
    };

    vkUpdateDescriptorSets(device->handle, 1, &sampler_write_set_info, 0, nullptr);

    return Sampler(impl);
}

auto Sampler::destroy() -> void {
    vkDestroySampler(impl->device->handle, impl->handle, nullptr);
    impl->device->resources.samplers.destroy(impl->id);
    std::erase_if(impl->device->resources.cached_samplers, [&](ls::pair<u64, SamplerID> &v) {  //
        return v.n1 == impl->id;
    });
}

auto Sampler::set_name(const std::string &name) -> Sampler & {
    set_object_name(impl->device, impl->handle, VK_OBJECT_TYPE_SAMPLER, name);

    return *this;
}

auto Sampler::id() const -> SamplerID {
    return impl->id;
}

}  // namespace lr
