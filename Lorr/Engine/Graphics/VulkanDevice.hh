#pragma once

#include "Engine/Graphics/Slang/Compiler.hh"
#include "Engine/Graphics/Vulkan.hh"

#include "Engine/Memory/SlotMap.hh"

#include <VkBootstrap.h>

#include <vuk/runtime/vk/Allocator.hpp>
#include <vuk/runtime/vk/AllocatorHelpers.hpp>
#include <vuk/runtime/vk/DeviceFrameResource.hpp>
#include <vuk/runtime/vk/DeviceLinearResource.hpp>
#include <vuk/runtime/vk/VkRuntime.hpp>
#include <vuk/vsl/Core.hpp>

namespace lr {
struct TransferManager {
private:
    Device *device = nullptr;

    mutable std::shared_mutex mutex = {};
    std::vector<vuk::UntypedValue> futures = {};
    plf::colony<vuk::Value<vuk::Buffer>> image_buffers = {};

    ls::option<vuk::Allocator> frame_allocator;

    friend Device;

public:
    TransferManager &operator=(const TransferManager &) = delete;
    TransferManager &operator=(TransferManager &&) = delete;

    auto init(Device &) -> std::expected<void, vuk::VkException>;
    auto destroy(this TransferManager &) -> void;

    [[nodiscard]]
    auto alloc_transient_buffer(this TransferManager &, vuk::MemoryUsage usage, usize size, LR_THISCALL) noexcept -> vuk::Value<vuk::Buffer>;

    [[nodiscard]]
    auto alloc_image_buffer(this TransferManager &, vuk::Format format, vuk::Extent3D extent, LR_THISCALL) noexcept -> vuk::Value<vuk::Buffer>;

    [[nodiscard]]
    auto upload(this TransferManager &, vuk::Value<vuk::Buffer> &&src, vuk::Value<vuk::Buffer> &&dst, LR_THISCALL) -> vuk::Value<vuk::Buffer>;

    [[nodiscard]]
    auto upload(this TransferManager &, vuk::Value<vuk::Buffer> &&src, vuk::Value<vuk::ImageAttachment> &&dst, LR_THISCALL)
        -> vuk::Value<vuk::ImageAttachment>;

    template<typename T>
    [[nodiscard]] auto upload(this TransferManager &self, ls::span<T> span, vuk::Value<vuk::Buffer> &&dst, LR_THISCALL) -> vuk::Value<vuk::Buffer> {
        ZoneScoped;

        auto src = self.alloc_transient_buffer(vuk::MemoryUsage::eCPUtoGPU, span.size_bytes(), LOC);
        std::memcpy(src->mapped_ptr, span.data(), span.size_bytes());
        return self.upload(std::move(src), std::move(dst), LOC);
    }

    template<typename T>
    [[nodiscard]] auto scratch_buffer(const T &val = {}, LR_THISCALL) -> vuk::Value<vuk::Buffer> {
        ZoneScoped;

        return scratch_buffer(&val, sizeof(T), LOC);
    }

    auto wait_on(this TransferManager &, vuk::UntypedValue &&fut) -> void;

protected:
    [[nodiscard]] auto scratch_buffer(this TransferManager &, const void *data, u64 size, LR_THISCALL) -> vuk::Value<vuk::Buffer>;
    auto wait_for_ops(this TransferManager &, vuk::Compiler &compiler) -> void;

    auto acquire(this TransferManager &, vuk::DeviceSuperFrameResource &super_frame_resource) -> void;
    auto release(this TransferManager &) -> void;
};

enum : u32 {
    DescriptorTable_SamplerIndex = 0,
    DescriptorTable_SampledImageIndex,
    DescriptorTable_StorageImageIndex,
};

struct DeviceResources {
    SlotMap<vuk::Buffer, BufferID> buffers = {};
    SlotMap<vuk::Image, ImageID> images = {};
    SlotMap<vuk::ImageView, ImageViewID> image_views = {};
    SlotMap<vuk::Sampler, SamplerID> samplers = {};
    SlotMap<vuk::PipelineBaseInfo *, PipelineID> pipelines = {};
    vuk::PersistentDescriptorSet descriptor_set = {};
};

struct Device {
    constexpr static auto MODULE_NAME = "Vulkan Device";

private:
    usize frames_in_flight = 0;
    ls::option<vuk::Runtime> runtime;
    ls::option<vuk::Allocator> allocator;
    ls::option<vuk::DeviceSuperFrameResource> frame_resources;
    vuk::Compiler compiler = {};
    TransferManager transfer_manager = {};
    SlangCompiler shader_compiler = {};
    DeviceResources resources = {};

    // Profiling tools
    ankerl::unordered_dense::map<vuk::Name, ls::pair<vuk::Query, vuk::Query>> pass_queries = {};

    vkb::Instance instance = {};
    vkb::PhysicalDevice physical_device = {};
    vkb::Device handle = {};

    VkPhysicalDeviceLimits device_limits = {};

    friend Buffer;
    friend Image;
    friend ImageView;
    friend Sampler;
    friend Pipeline;
    friend TransferManager;

public:
    Device(usize frame_count_) : frames_in_flight(frame_count_) {}
    auto init(this Device &) -> bool;
    auto init_resources(this Device &) -> std::expected<void, vuk::VkException>;
    auto destroy(this Device &) -> void;

    auto new_slang_session(this Device &, const SlangSessionInfo &info) -> ls::option<SlangSession>;

    auto transfer_man(this Device &) -> TransferManager &;
    auto new_frame(this Device &, vuk::Swapchain &) -> vuk::Value<vuk::ImageAttachment>;
    auto end_frame(this Device &, vuk::Value<vuk::ImageAttachment> &&target_attachment) -> void;
    auto wait(this Device &, LR_THISCALL) -> void;

    auto create_persistent_descriptor_set(
        this Device &,
        u32 set_index,
        ls::span<VkDescriptorSetLayoutBinding> bindings,
        ls::span<VkDescriptorBindingFlags> binding_flags
    ) -> vuk::PersistentDescriptorSet;
    auto commit_descriptor_set(this Device &, ls::span<VkWriteDescriptorSet> writes) -> void;
    auto create_swap_chain(this Device &, VkSurfaceKHR surface, ls::option<vuk::Swapchain> old_swap_chain = ls::nullopt)
        -> std::expected<vuk::Swapchain, vuk::VkException>;

    auto set_name(this Device &, Buffer &buffer, std::string_view name) -> void;
    auto set_name(this Device &, Image &image, std::string_view name) -> void;
    auto set_name(this Device &, ImageView &image_view, std::string_view name) -> void;

    auto frame_count(this const Device &) -> usize;
    auto buffer(this Device &, BufferID) -> ls::option<vuk::Buffer>;
    auto image(this Device &, ImageID) -> ls::option<vuk::Image>;
    auto image_view(this Device &, ImageViewID) -> ls::option<vuk::ImageView>;
    auto sampler(this Device &, SamplerID) -> ls::option<vuk::Sampler>;
    auto pipeline(this Device &, PipelineID) -> vuk::PipelineBaseInfo **;

    auto destroy(this Device &, BufferID) -> void;
    auto destroy(this Device &, ImageID) -> void;
    auto destroy(this Device &, ImageViewID) -> void;
    auto destroy(this Device &, SamplerID) -> void;
    auto destroy(this Device &, PipelineID) -> void;

    struct Limits {
        constexpr static u32 FrameCount = 3;
    };

    auto get_instance() -> VkInstance {
        return instance.instance;
    }
    auto get_native_handle() -> VkDevice {
        return handle.device;
    }
    auto get_allocator() -> vuk::Allocator & {
        return allocator.value();
    }
    auto get_compiler() -> vuk::Compiler & {
        return compiler;
    }
    auto get_runtime() -> vuk::Runtime & {
        return *runtime;
    }
    auto get_pass_queries() -> auto & {
        return pass_queries;
    }
    auto get_descriptor_set() -> auto & {
        return resources.descriptor_set;
    }

    auto non_coherent_atom_size() -> u32 {
        return device_limits.nonCoherentAtomSize;
    }
};
} // namespace lr
