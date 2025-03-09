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
    this TransferManager &self, void *data, u64 data_size, vuk::Value<vuk::Buffer> &&dst, u64 dst_offset, vuk::source_location LOC)
    -> vuk::Value<vuk::Buffer> {
    ZoneScoped;

    auto cpu_buffer = self.alloc_transient_buffer(vuk::MemoryUsage::eCPUonly, data_size, LOC);
    std::memcpy(cpu_buffer->mapped_ptr, data, data_size);

    auto dst_buffer = vuk::discard_buf("dst", dst->subrange(dst_offset, cpu_buffer->size), LOC);
    return self.upload_staging(std::move(cpu_buffer), std::move(dst_buffer), LOC);
}

auto TransferManager::upload_staging(
    this TransferManager &self, void *data, u64 data_size, Buffer &dst, u64 dst_offset, vuk::source_location LOC)
    -> vuk::Value<vuk::Buffer> {
    ZoneScoped;

    auto cpu_buffer = self.alloc_transient_buffer(vuk::MemoryUsage::eCPUonly, data_size, LOC);
    std::memcpy(cpu_buffer->mapped_ptr, data, data_size);

    auto *dst_handle = self.device->buffer(dst.id());
    auto dst_buffer = vuk::discard_buf("dst", dst_handle->subrange(dst_offset, cpu_buffer->size), LOC);
    return self.upload_staging(std::move(cpu_buffer), std::move(dst_buffer), LOC);
}

auto TransferManager::upload_staging(this TransferManager &self, ImageView &image_view, void *data, u64 data_size, vuk::source_location LOC)
    -> vuk::Value<vuk::ImageAttachment> {
    ZoneScoped;

    auto dst_attachment_info = image_view.get_attachment(*self.device, vuk::ImageUsageFlagBits::eTransferDst);
    auto result =
        vuk::host_data_to_image(self.device->allocator.value(), vuk::DomainFlagBits::eGraphicsQueue, dst_attachment_info, data, LOC);
    result = vuk::generate_mips(std::move(result), 0, image_view.mip_count() - 1);
    return result;
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

auto TransferManager::wait_for_ops(this TransferManager &self, vuk::Compiler &compiler) -> void {
    ZoneScoped;

    vuk::wait_for_values_explicit(*self.frame_allocator, compiler, self.futures, {});
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
