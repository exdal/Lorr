#pragma once

#include "Engine/Graphics/Slang/Compiler.hh"
#include "Engine/Graphics/Vulkan.hh"

#include "Engine/Memory/SlotMap.hh"

#include "Engine/Util/LegitProfiler.hh"

#include <VkBootstrap.h>

#include <vuk/runtime/vk/Allocator.hpp>
#include <vuk/runtime/vk/AllocatorHelpers.hpp>
#include <vuk/runtime/vk/DeviceFrameResource.hpp>
#include <vuk/runtime/vk/DeviceLinearResource.hpp>
#include <vuk/runtime/vk/VkRuntime.hpp>
#include <vuk/vsl/Core.hpp>

namespace lr {
struct TransientBuffer {
    vuk::Buffer buffer = {};

    u64 device_address() { return buffer.device_address; }
    template<typename T = u8>
    T *host_ptr() {
        return reinterpret_cast<T *>(buffer.mapped_ptr);
    }
};

struct TransferManager {
    TransferManager &operator=(const TransferManager &) = delete;
    TransferManager &operator=(TransferManager &&) = delete;

    auto init(Device &) -> std::expected<void, vuk::VkException>;
    auto destroy(this TransferManager &) -> void;

    auto alloc_transient_buffer(this TransferManager &, vuk::MemoryUsage usage, usize size) -> TransientBuffer;

    auto upload_staging(this TransferManager &, Buffer &buffer, ls::span<u8> bytes, u64 offset, u64 element_size) -> void;
    auto upload_staging(this TransferManager &, TransientBuffer &src, Buffer &dst) -> void;
    auto upload_staging(this TransferManager &, TransientBuffer &src, TransientBuffer &dst) -> void;
    auto upload_staging(this TransferManager &, ImageView &image_view, ls::span<u8> bytes) -> void;

    auto wait_on(this TransferManager &, vuk::UntypedValue &&fut) -> void;

protected:
    auto wait_for_ops(this TransferManager &, vuk::Allocator &allocator, vuk::Compiler &compiler) -> void;

    auto acquire(this TransferManager &, vuk::DeviceFrameResource &allocator) -> void;
    auto release(this TransferManager &) -> void;

private:
    Device *device = nullptr;
    std::vector<vuk::UntypedValue> futures = {};

    ls::option<vuk::Allocator> frame_allocator;

    friend Device;
};

struct BindlessDescriptorSetLayoutInfo {
    BindlessDescriptorSetLayoutInfo() { layout_info.index = 0; }
    vuk::DescriptorSetLayoutCreateInfo layout_info = {};
    usize pool_size = 0;

    auto add_binding(Descriptors binding, VkDescriptorType type, u32 count) -> void;
};

struct DeviceResources {
    SlotMap<vuk::Unique<vuk::Buffer>, BufferID> buffers = {};
    SlotMap<vuk::Unique<vuk::Image>, ImageID> images = {};
    SlotMap<vuk::Unique<vuk::ImageView>, ImageViewID> image_views = {};
    SlotMap<vuk::Sampler, SamplerID> samplers = {};
    SlotMap<vuk::PipelineBaseInfo *, PipelineID> pipelines = {};
};

enum class DeviceFeature : u64 {
    None = 0,
    DescriptorBuffer = 1 << 0,
    MemoryBudget = 1 << 1,
    QueryTimestamp = 1 << 2,
};
consteval void enable_bitmask(DeviceFeature);

struct Device {
    auto init(this Device &, usize frame_count) -> std::expected<void, vuk::VkException>;
    auto destroy(this Device &) -> void;

    auto new_slang_session(this Device &, const SlangSessionInfo &info) -> ls::option<SlangSession>;

    auto transfer_man(this Device &) -> TransferManager &;
    auto new_frame(this Device &, SwapChain &) -> vuk::Value<vuk::ImageAttachment>;
    auto end_frame(this Device &, vuk::Value<vuk::ImageAttachment> &&target_attachment) -> void;
    auto wait() -> void;

    auto set_name(this Device &, Buffer &buffer, std::string_view name) -> void;
    auto set_name(this Device &, Image &image, std::string_view name) -> void;
    auto set_name(this Device &, ImageView &image_view, std::string_view name) -> void;

    auto frame_count(this const Device &) -> usize;
    auto buffer(this Device &, BufferID) -> vuk::Buffer *;
    auto image(this Device &, ImageID) -> vuk::Image *;
    auto image_view(this Device &, ImageViewID) -> vuk::ImageView *;
    auto sampler(this Device &, SamplerID) -> vuk::Sampler *;
    auto pipeline(this Device &, PipelineID) -> vuk::PipelineBaseInfo **;
    auto bindless_descriptor_set() -> vuk::PersistentDescriptorSet &;

    auto feature_supported(this Device &, DeviceFeature feature) -> bool;

    auto get_bindless_descriptor_set_layout() -> BindlessDescriptorSetLayoutInfo;

    struct Limits {
        constexpr static u32 FrameCount = 3;
    };

    auto get_instance() -> VkInstance { return instance.instance; }

private:
    ls::option<vuk::Runtime> runtime;
    ls::option<vuk::Allocator> allocator;
    ls::option<vuk::DeviceSuperFrameResource> frame_resources;
    vuk::Compiler compiler = {};

    usize frames_in_flight = 0;
    vuk::Unique<vuk::PersistentDescriptorSet> descriptor_set;
    Buffer bda_array_buffer = {};
    TransferManager transfer_manager = {};

    SlangCompiler shader_compiler = {};
    DeviceFeature supported_features = DeviceFeature::None;
    DeviceResources resources = {};

    // Profiling tools
    std::vector<legit::ProfilerTask> gpu_profiler_tasks = {};
    legit::ProfilerGraph gpu_profiler_graph = { 400 };

    vkb::Instance instance = {};
    vkb::PhysicalDevice physical_device = {};
    vkb::Device handle = {};

    friend Buffer;
    friend Image;
    friend ImageView;
    friend Sampler;
    friend Pipeline;
    friend SwapChain;

    friend TransferManager;
};
}  // namespace lr
