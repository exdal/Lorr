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

auto TransferManager::upload_staging(this TransferManager &self, Buffer &buffer, ls::span<u8> bytes) -> void {
    ZoneScoped;

    auto cpu_buffer = self.alloc_transient_buffer(vuk::MemoryUsage::eCPUonly, bytes.size_bytes());
    std::memcpy(cpu_buffer.host_ptr(), bytes.data(), bytes.size_bytes());

    auto *dst_handle = self.device->buffer(buffer.id());
    auto src_buffer = vuk::acquire_buf("src", cpu_buffer.buffer, vuk::Access::eNone);
    auto dst_buffer = vuk::discard_buf("dst", *dst_handle);
    auto upload_pass = vuk::make_pass(
        "TransferManager::buffer_upload",
        [](vuk::CommandBuffer &cmd_list, VUK_BA(vuk::Access::eTransferRead) src, VUK_BA(vuk::Access::eTransferWrite) dst) {
            cmd_list.copy_buffer(src, dst);
            return dst;
        });

    self.wait_on(std::move(upload_pass(std::move(src_buffer), std::move(dst_buffer))));
}

auto TransferManager::upload_staging(this TransferManager &self, TransientBuffer &src, TransientBuffer &dst) -> void {
    ZoneScoped;

    auto src_buffer = vuk::acquire_buf("src", src.buffer, vuk::Access::eNone);
    auto dst_buffer = vuk::discard_buf("dst", dst.buffer);
    auto upload_pass = vuk::make_pass(
        "TransferManager::buffer_to_buffer",
        [](vuk::CommandBuffer &cmd_list, VUK_BA(vuk::Access::eTransferRead) src_, VUK_BA(vuk::Access::eTransferWrite) dst_) {
            cmd_list.copy_buffer(src_, dst_);
            return dst_;
        });

    self.wait_on(std::move(upload_pass(std::move(src_buffer), std::move(dst_buffer))));
}

auto TransferManager::upload_staging(this TransferManager &self, ImageView &image_view, ls::span<u8> bytes) -> void {
    ZoneScoped;

    auto cpu_buffer = self.alloc_transient_buffer(vuk::MemoryUsage::eCPUonly, bytes.size_bytes());
    std::memcpy(cpu_buffer.host_ptr(), bytes.data(), bytes.size_bytes());

    vuk::ImageSubresourceLayers target_layer = {
        .aspectMask = vuk::format_to_aspect(image_view.format()),
        .mipLevel = 0,
        .baseArrayLayer = 0,
        .layerCount = 1,
    };
    vuk::BufferImageCopy copy_region = {
        .bufferOffset = cpu_buffer.buffer.offset,
        .imageSubresource = target_layer,
        .imageExtent = image_view.extent(),
    };

    auto dst_attachment_info = image_view.get_attachment(*self.device, vuk::ImageUsageFlagBits::eTransferDst);
    auto src_buffer = vuk::acquire_buf("src", cpu_buffer.buffer, vuk::Access::eNone);
    auto dst_attachment = vuk::declare_ia("dst", dst_attachment_info);
    auto upload_pass = vuk::make_pass(
        "TransferManager::image_upload",
        [copy_region](vuk::CommandBuffer &cmd_list, VUK_BA(vuk::Access::eTransferRead) src, VUK_IA(vuk::Access::eTransferWrite) dst) {
            cmd_list.copy_buffer_to_image(src, dst, copy_region);
            return dst;
        });

    dst_attachment = upload_pass(std::move(src_buffer), std::move(dst_attachment));
    if (image_view.mip_count() > 1) {
        dst_attachment = vuk::generate_mips(std::move(dst_attachment), 0, image_view.mip_count());
    }

    self.wait_on(std::move(dst_attachment));
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
