#include "Engine/Graphics/VulkanDevice.hh"

namespace lr {
auto TransferManager::init(Device &device_) -> std::expected<void, vuk::VkException> {
    ZoneScoped;

    this->device = &device_;

    return {};
}

auto TransferManager::destroy(this TransferManager &self) -> void {
    ZoneScoped;

    self.release();
}

auto TransferManager::alloc_transient_buffer_raw(this TransferManager &self, vuk::MemoryUsage usage, usize size, vuk::source_location LOC)
    -> vuk::Buffer {
    ZoneScoped;

    auto *allocator = &self.device->allocator.value();
    if (self.frame_allocator.has_value()) {
        allocator = &self.frame_allocator.value();
    }

    auto buffer = *vuk::allocate_buffer(*allocator, { .mem_usage = usage, .size = size, .alignment = 8 }, LOC);
    return *buffer;
}

auto TransferManager::alloc_transient_buffer(this TransferManager &self, vuk::MemoryUsage usage, usize size, vuk::source_location LOC)
    -> vuk::Value<vuk::Buffer> {
    ZoneScoped;

    auto buffer = self.alloc_transient_buffer_raw(usage, size, LOC);
    return vuk::acquire_buf("transient buffer", buffer, vuk::Access::eNone, LOC);
}

auto TransferManager::upload_staging(
    this TransferManager &, vuk::Value<vuk::Buffer> &&src, vuk::Value<vuk::Buffer> &&dst, vuk::source_location LOC)
    -> vuk::Value<vuk::Buffer> {
    ZoneScoped;

    auto upload_pass = vuk::make_pass(
        "upload staging",
        [](vuk::CommandBuffer &cmd_list, VUK_BA(vuk::Access::eTransferRead) src_ba, VUK_BA(vuk::Access::eTransferWrite) dst_ba) {
            cmd_list.copy_buffer(src_ba, dst_ba);
            return dst_ba;
        },
        vuk::DomainFlagBits::eAny,
        LOC);

    return upload_pass(std::move(src), std::move(dst));
}

auto TransferManager::upload_staging(
    this TransferManager &self, vuk::Value<vuk::Buffer> &&src, Buffer &dst, u64 dst_offset, vuk::source_location LOC)
    -> vuk::Value<vuk::Buffer> {
    ZoneScoped;

    auto *dst_handle = self.device->buffer(dst.id());
    auto dst_buffer = vuk::discard_buf("dst", dst_handle->subrange(dst_offset, src->size), LOC);
    return self.upload_staging(std::move(src), std::move(dst_buffer), LOC);
}

auto TransferManager::upload_staging(
    this TransferManager &self, ls::span<u8> bytes, vuk::Value<vuk::Buffer> &&dst, vuk::source_location LOC) -> vuk::Value<vuk::Buffer> {
    ZoneScoped;

    auto cpu_buffer = self.alloc_transient_buffer(vuk::MemoryUsage::eCPUonly, bytes.size_bytes(), LOC);
    std::memcpy(cpu_buffer->mapped_ptr, bytes.data(), bytes.size_bytes());
    return self.upload_staging(std::move(cpu_buffer), std::move(dst), LOC);
}

auto TransferManager::upload_staging(this TransferManager &self, ls::span<u8> bytes, Buffer &dst, u64 dst_offset, vuk::source_location LOC)
    -> vuk::Value<vuk::Buffer> {
    ZoneScoped;

    auto cpu_buffer = self.alloc_transient_buffer(vuk::MemoryUsage::eCPUonly, bytes.size_bytes(), LOC);
    std::memcpy(cpu_buffer->mapped_ptr, bytes.data(), bytes.size_bytes());

    auto *dst_handle = self.device->buffer(dst.id());
    auto dst_buffer = vuk::discard_buf("dst", dst_handle->subrange(dst_offset, cpu_buffer->size), LOC);
    return self.upload_staging(std::move(cpu_buffer), std::move(dst_buffer), LOC);
}

auto TransferManager::upload_staging(this TransferManager &self, ImageView &image_view, ls::span<u8> bytes, vuk::source_location LOC)
    -> vuk::Value<vuk::ImageAttachment> {
    ZoneScoped;

    auto cpu_buffer = self.alloc_transient_buffer(vuk::MemoryUsage::eCPUonly, bytes.size_bytes(), LOC);
    std::memcpy(cpu_buffer->mapped_ptr, bytes.data(), bytes.size_bytes());

    auto dst_attachment_info = image_view.get_attachment(*self.device, vuk::ImageUsageFlagBits::eTransferDst);
    auto dst_attachment = vuk::declare_ia("dst", dst_attachment_info, LOC);

    vuk::ImageSubresourceLayers target_layer = {
        .aspectMask = vuk::format_to_aspect(image_view.format()),
        .mipLevel = 0,
        .baseArrayLayer = 0,
        .layerCount = 1,
    };
    vuk::BufferImageCopy copy_region = {
        .bufferOffset = cpu_buffer->offset,
        .imageSubresource = target_layer,
        .imageExtent = dst_attachment->base_mip_extent(),
    };

    auto upload_pass = vuk::make_pass(
        "TransferManager::image_upload",
        [copy_region](vuk::CommandBuffer &cmd_list, VUK_BA(vuk::Access::eTransferRead) src_ba, VUK_IA(vuk::Access::eTransferWrite) dst_ia) {
            cmd_list.copy_buffer_to_image(src_ba, dst_ia, copy_region);
            return dst_ia;
        });

    dst_attachment = upload_pass(std::move(cpu_buffer), std::move(dst_attachment), LOC);
    if (image_view.mip_count() > 1) {
        dst_attachment = vuk::generate_mips(std::move(dst_attachment), 0, image_view.mip_count() - 1);
    }

    return dst_attachment;
}

auto TransferManager::scratch_buffer(this TransferManager &self, const void *data, u64 size, vuk::source_location LOC)
    -> vuk::Value<vuk::Buffer> {
    ZoneScoped;

    auto cpu_buffer = self.alloc_transient_buffer(vuk::MemoryUsage::eCPUonly, size, LOC);
    std::memcpy(cpu_buffer->mapped_ptr, data, size);
    auto gpu_buffer = self.alloc_transient_buffer(vuk::MemoryUsage::eGPUonly, size, LOC);

    auto upload_pass = vuk::make_pass(
        "TransferManager::scratch_buffer",
        [](vuk::CommandBuffer &cmd_list, VUK_BA(vuk::Access::eTransferRead) src, VUK_BA(vuk::Access::eTransferWrite) dst) {
            cmd_list.copy_buffer(src, dst);
            return dst;
        },
        vuk::DomainFlagBits::eAny,
        LOC);

    return upload_pass(std::move(cpu_buffer), std::move(gpu_buffer));
}

auto TransferManager::wait_on(this TransferManager &self, vuk::UntypedValue &&fut) -> void {
    ZoneScoped;

    self.futures.emplace_back(std::move(fut));
}

auto TransferManager::resize_buffer(this TransferManager &self, Buffer &buffer, u64 new_size) -> Buffer {
    ZoneScoped;

    LS_EXPECT(buffer);
    auto *old_buffer_handle = self.device->buffer(buffer.id());
    if (old_buffer_handle->size < new_size) {
        ZoneScopedN("Buffer resize");
        // Device wait here is important, do not remove it. Why?
        // We are using ONE transform buffer for all frames, if
        // this buffer gets destroyed in current frame, previous
        // rendering frame buffer will get corrupt and crash GPU.
        self.device->wait();
        auto new_buffer = Buffer::create(*self.device, new_size, old_buffer_handle->memory_usage).value();
        auto new_buffer_handle = self.device->buffer(new_buffer.id());
        auto new_buffer_subrange = new_buffer_handle->subrange(0, old_buffer_handle->size);

        auto old_buffer_val = vuk::acquire_buf("old resizing buffer", *old_buffer_handle, vuk::Access::eNone);
        auto new_buffer_val = vuk::discard_buf("new resizing buffer subrance", new_buffer_subrange);
        self.upload_staging(std::move(old_buffer_val), std::move(new_buffer_val))  //
            .wait(*self.device->allocator, self.device->compiler);

        self.device->destroy(buffer.id());

        return new_buffer;
    }

    // No new allocations needed, just return back
    return buffer;
}

auto TransferManager::wait_for_ops(this TransferManager &self, vuk::Allocator &allocator, vuk::Compiler &compiler) -> void {
    ZoneScoped;

    vuk::wait_for_values_explicit(allocator, compiler, self.futures, {});
    self.futures.clear();
}

auto TransferManager::acquire(this TransferManager &self, vuk::DeviceFrameResource &allocator) -> void {
    ZoneScoped;

    self.frame_allocator.emplace(allocator);
}

auto TransferManager::release(this TransferManager &self) -> void {
    ZoneScoped;

    self.frame_allocator.reset();
}

}  // namespace lr
