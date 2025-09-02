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

auto TransferManager::alloc_transient_buffer(this TransferManager &self, vuk::MemoryUsage usage, usize size, vuk::source_location LOC) noexcept
    -> vuk::Value<vuk::Buffer> {
    ZoneScoped;

    auto buffer = vuk::Buffer{};
    auto buffer_info = vuk::BufferCreateInfo{ .mem_usage = usage, .size = size, .alignment = self.device->non_coherent_atom_size() };
    self.frame_allocator->allocate_buffers({ &buffer, 1 }, { &buffer_info, 1 }, LOC);

    return vuk::acquire_buf("transient buffer", buffer, vuk::eNone, LOC);
}

auto TransferManager::alloc_image_buffer(this TransferManager &self, vuk::Format format, vuk::Extent3D extent, vuk::source_location LOC) noexcept
    -> vuk::Value<vuk::Buffer> {
    ZoneScoped;

    auto write_lock = std::unique_lock(self.mutex);
    auto alignment = vuk::format_to_texel_block_size(format);
    auto size = vuk::compute_image_size(format, extent);

    auto buffer_handle = vuk::Buffer{};
    auto buffer_info = vuk::BufferCreateInfo{ .mem_usage = vuk::MemoryUsage::eCPUtoGPU, .size = size, .alignment = alignment };
    self.device->allocator->allocate_buffers({ &buffer_handle, 1 }, { &buffer_info, 1 }, LOC);

    auto buffer = vuk::acquire_buf("image buffer", buffer_handle, vuk::eNone, LOC);
    self.image_buffers.emplace(buffer);

    return buffer;
}

auto TransferManager::upload(this TransferManager &, vuk::Value<vuk::Buffer> &&src, vuk::Value<vuk::Buffer> &&dst, vuk::source_location LOC)
    -> vuk::Value<vuk::Buffer> {
    ZoneScoped;

    auto upload_pass = vuk::make_pass(
        "upload staging",
        [](vuk::CommandBuffer &cmd_list, VUK_BA(vuk::Access::eTransferRead) src_ba, VUK_BA(vuk::Access::eTransferWrite) dst_ba) {
            cmd_list.copy_buffer(src_ba, dst_ba);
            return dst_ba;
        },
        vuk::DomainFlagBits::eAny,
        LOC
    );

    return upload_pass(std::move(src), std::move(dst));
}

[[nodiscard]]
auto TransferManager::upload(this TransferManager &, vuk::Value<vuk::Buffer> &&src, vuk::Value<vuk::ImageAttachment> &&dst, vuk::source_location LOC)
    -> vuk::Value<vuk::ImageAttachment> {
    ZoneScoped;

    auto upload_pass = vuk::make_pass(
        "upload",
        [](vuk::CommandBuffer &cmd_list, //
           VUK_BA(vuk::eTransferRead) src,
           VUK_IA(vuk::eTransferWrite) dst) {
            auto buffer_copy_region = vuk::BufferImageCopy{
                .bufferOffset = src->offset,
                .imageSubresource = { .aspectMask = vuk::ImageAspectFlagBits::eColor, .layerCount = 1 },
                .imageOffset = {},
                .imageExtent = dst->extent,
            };
            cmd_list.copy_buffer_to_image(src, dst, buffer_copy_region);
            return dst;
        },
        vuk::DomainFlagBits::eAny,
        LOC
    );

    return upload_pass(std::move(src), std::move(dst));
}

auto TransferManager::scratch_buffer(this TransferManager &self, const void *data, u64 size, vuk::source_location LOC) -> vuk::Value<vuk::Buffer> {
    ZoneScoped;

#define SCRATCH_BUFFER_USE_BAR
#ifndef SCRATCH_BUFFER_USE_BAR
    auto cpu_buffer = self.alloc_transient_buffer(vuk::MemoryUsage::eCPUonly, size, LOC);
    std::memcpy(cpu_buffer->mapped_ptr, data, size);
    auto gpu_buffer = self.alloc_transient_buffer(vuk::MemoryUsage::eGPUonly, size, LOC);

    auto upload_pass = vuk::make_pass(
        "scratch_buffer",
        [](vuk::CommandBuffer &cmd_list, VUK_BA(vuk::Access::eTransferRead) src, VUK_BA(vuk::Access::eTransferWrite) dst) {
            cmd_list.copy_buffer(src, dst);
            return dst;
        },
        vuk::DomainFlagBits::eAny,
        LOC
    );

    return upload_pass(std::move(cpu_buffer), std::move(gpu_buffer));
#else
    auto buffer = self.alloc_transient_buffer(vuk::MemoryUsage::eGPUtoCPU, size, LOC);
    std::memcpy(buffer->mapped_ptr, data, size);
    return buffer;
#endif
}

auto TransferManager::wait_on(this TransferManager &self, vuk::UntypedValue &&fut) -> void {
    ZoneScoped;

#define WAIT_ON_HOST
#ifdef WAIT_ON_HOST
    thread_local vuk::Compiler transfer_man_compiler;
    fut.wait(self.device->get_allocator(), transfer_man_compiler);
#else
    auto write_lock = std::unique_lock(self.mutex);
    self.futures.push_back(std::move(fut));
#endif
}

auto TransferManager::wait_for_ops(this TransferManager &self, vuk::Compiler &compiler) -> void {
    ZoneScoped;

    auto write_lock = std::unique_lock(self.mutex);
    vuk::wait_for_values_explicit(*self.frame_allocator, compiler, self.futures, {});
    self.futures.clear();
}

auto TransferManager::acquire(this TransferManager &self, vuk::DeviceSuperFrameResource &super_frame_resource) -> void {
    ZoneScoped;

    auto write_lock = std::unique_lock(self.mutex);
    auto &frame_resource = super_frame_resource.get_next_frame();
    self.frame_allocator.emplace(frame_resource);

    for (auto it = self.image_buffers.begin(); it != self.image_buffers.end();) {
        auto image_buffer = &*it;
        if (*image_buffer->poll() == vuk::Signal::Status::eHostAvailable) {
            auto evaluated_buffer = vuk::eval<vuk::Buffer>(image_buffer->get_head());
            LS_EXPECT(evaluated_buffer.holds_value());
            self.device->allocator->deallocate({ &evaluated_buffer.value(), 1 });
            it = self.image_buffers.erase(it);
            continue;
        }

        ++it;
    }
}

auto TransferManager::release(this TransferManager &self) -> void {
    ZoneScoped;

    auto write_lock = std::unique_lock(self.mutex);
    self.frame_allocator.reset();
}

} // namespace lr
