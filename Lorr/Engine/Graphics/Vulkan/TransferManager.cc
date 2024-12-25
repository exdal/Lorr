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

    // Make sure to call this inside frame mark
    LS_EXPECT(self.frame_allocator.has_value());

    auto buffer = vuk::allocate_buffer(*self.frame_allocator, { usage, size, 8 });
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
        .imageSubresource = target_layer,
        .imageExtent = image_view.extent(),
    };

    auto dst_attachment = image_view.get_attachment(*self.device, vuk::ImageUsageFlagBits::eTransferDst);
    auto src_buffer = vuk::acquire_buf("src", cpu_buffer.buffer, vuk::Access::eNone);
    auto dst_image = vuk::declare_ia("dst", dst_attachment);
    auto upload_pass = vuk::make_pass(
        "TransferManager::image_upload",
        [copy_region](vuk::CommandBuffer &cmd_list, VUK_BA(vuk::Access::eTransferRead) src, VUK_IA(vuk::Access::eTransferWrite) dst) {
            cmd_list.copy_buffer_to_image(src, dst, copy_region);
            return dst;
        });

    self.wait_on(std::move(upload_pass(std::move(src_buffer), std::move(dst_image))));
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
