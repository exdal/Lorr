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

auto TransferManager::alloc_transient_buffer(this TransferManager &self, vuk::MemoryUsage usage, usize size) -> TransientBuffer {
    ZoneScoped;

    auto *allocator = &self.device->allocator.value();
    if (self.frame_allocator.has_value()) {
        allocator = &self.frame_allocator.value();
    }

    auto buffer = vuk::allocate_buffer(*allocator, { .mem_usage = usage, .size = size, .alignment = 8 });
    if (!buffer.holds_value()) {
        LOG_ERROR("Transient Buffer Error: {}", buffer.error().error_message);
        return {};
    }

    return TransientBuffer{ .buffer = buffer->get() };
}

auto TransferManager::upload_staging(this TransferManager &self, ls::span<u8> bytes, Buffer &dst, u64 dst_offset) -> void {
    ZoneScoped;

    auto cpu_buffer = self.alloc_transient_buffer(vuk::MemoryUsage::eCPUonly, bytes.size_bytes());
    std::memcpy(cpu_buffer.host_ptr(), bytes.data(), bytes.size_bytes());

    auto *dst_handle = self.device->buffer(dst.id());
    auto src_buffer = vuk::acquire_buf("src", cpu_buffer.buffer, vuk::Access::eNone);
    auto dst_buffer = vuk::discard_buf("dst", dst_handle->subrange(dst_offset, cpu_buffer.buffer.size));
    auto upload_pass = vuk::make_pass(
        "TransferManager::buffer_upload",
        [](vuk::CommandBuffer &cmd_list, VUK_BA(vuk::Access::eTransferRead) src_ba, VUK_BA(vuk::Access::eTransferWrite) dst_ba) {
            cmd_list.copy_buffer(src_ba, dst_ba);
            return dst_ba;
        });

    self.wait_on(std::move(upload_pass(std::move(src_buffer), std::move(dst_buffer))));
}

auto TransferManager::upload_staging(this TransferManager &self, TransientBuffer &src, Buffer &dst, u64 dst_offset) -> void {
    ZoneScoped;

    auto *dst_handle = self.device->buffer(dst.id());
    auto src_buffer = vuk::acquire_buf("src", src.buffer, vuk::Access::eNone);
    auto dst_buffer = vuk::discard_buf("dst", dst_handle->subrange(dst_offset, src.buffer.size));
    auto upload_pass = vuk::make_pass(
        "TransferManager::transient_buffer_to_buffer",
        [](vuk::CommandBuffer &cmd_list, VUK_BA(vuk::Access::eTransferRead) src_ba, VUK_BA(vuk::Access::eTransferWrite) dst_ba) {
            cmd_list.copy_buffer(src_ba, dst_ba);
            return dst_ba;
        });

    self.wait_on(std::move(upload_pass(std::move(src_buffer), std::move(dst_buffer))));
}

auto TransferManager::upload_staging(this TransferManager &self, TransientBuffer &src, TransientBuffer &dst) -> void {
    ZoneScoped;

    auto src_buffer = vuk::acquire_buf("src", src.buffer, vuk::Access::eNone);
    auto dst_buffer = vuk::discard_buf("dst", dst.buffer);
    auto upload_pass = vuk::make_pass(
        "TransferManager::buffer_to_buffer",
        [](vuk::CommandBuffer &cmd_list, VUK_BA(vuk::Access::eTransferRead) src_ba, VUK_BA(vuk::Access::eTransferWrite) dst_ba) {
            cmd_list.copy_buffer(src_ba, dst_ba);
            return dst_ba;
        });

    self.wait_on(std::move(upload_pass(std::move(src_buffer), std::move(dst_buffer))));
}

auto TransferManager::upload_staging(this TransferManager &self, ImageView &image_view, ls::span<u8> bytes) -> void {
    ZoneScoped;

    auto buffer = vuk::allocate_buffer(
                      *self.device->allocator, { .mem_usage = vuk::MemoryUsage::eCPUonly, .size = bytes.size_bytes(), .alignment = 8 })
                      .value();
    std::memcpy(buffer->mapped_ptr, bytes.data(), bytes.size_bytes());

    auto dst_attachment_info = image_view.get_attachment(*self.device, vuk::ImageUsageFlagBits::eTransferDst);
    auto src_buffer = vuk::acquire_buf("src", *buffer, vuk::Access::eNone);
    auto dst_attachment = vuk::declare_ia("dst", dst_attachment_info);

    vuk::ImageSubresourceLayers target_layer = {
        .aspectMask = vuk::format_to_aspect(image_view.format()),
        .mipLevel = 0,
        .baseArrayLayer = 0,
        .layerCount = 1,
    };
    vuk::BufferImageCopy copy_region = {
        .bufferOffset = src_buffer->offset,
        .imageSubresource = target_layer,
        .imageExtent = dst_attachment->base_mip_extent(),
    };

    auto upload_pass = vuk::make_pass(
        "TransferManager::image_upload",
        [copy_region](vuk::CommandBuffer &cmd_list, VUK_BA(vuk::Access::eTransferRead) src_ba, VUK_IA(vuk::Access::eTransferWrite) dst_ia) {
            cmd_list.copy_buffer_to_image(src_ba, dst_ia, copy_region);
            return dst_ia;
        });

    dst_attachment = upload_pass(std::move(src_buffer), std::move(dst_attachment));
    if (image_view.mip_count() > 1) {
        dst_attachment = vuk::generate_mips(std::move(dst_attachment), 0, image_view.mip_count() - 1);
    }

    self.wait_on(std::move(dst_attachment));
}

auto TransferManager::scratch_buffer(this TransferManager &self, const void *data, u64 size) -> vuk::Value<vuk::Buffer> {
    ZoneScoped;

    auto cpu_buffer = self.alloc_transient_buffer(vuk::MemoryUsage::eCPUonly, size);
    std::memcpy(cpu_buffer.host_ptr(), data, size);

    auto gpu_buffer = self.alloc_transient_buffer(vuk::MemoryUsage::eGPUonly, size);

    auto src_buffer = vuk::acquire_buf("src", cpu_buffer.buffer, vuk::Access::eNone);
    auto dst_buffer = vuk::acquire_buf("dst", gpu_buffer.buffer, vuk::Access::eNone);
    auto upload_pass = vuk::make_pass(
        "TransferManager::scratch_buffer",
        [](vuk::CommandBuffer &cmd_list, VUK_BA(vuk::Access::eTransferRead) src, VUK_BA(vuk::Access::eTransferWrite) dst) {
            cmd_list.copy_buffer(src, dst);
            return dst;
        });

    return upload_pass(std::move(src_buffer), std::move(dst_buffer));
}

auto TransferManager::wait_on(this TransferManager &self, vuk::UntypedValue &&fut) -> void {
    ZoneScoped;

    self.futures.emplace_back(std::move(fut));
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
