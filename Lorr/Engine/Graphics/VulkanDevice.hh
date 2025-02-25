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
struct BindlessDescriptorInfo {
    u32 binding = 0;
    vuk::DescriptorType type = {};
    u32 descriptor_count = 0;
};

struct TransferManager {
private:
    Device *device = nullptr;
    std::vector<vuk::UntypedValue> futures = {};

    ls::option<vuk::Allocator> frame_allocator;

    friend Device;

public:
    TransferManager &operator=(const TransferManager &) = delete;
    TransferManager &operator=(TransferManager &&) = delete;

    auto init(Device &) -> std::expected<void, vuk::VkException>;
    auto destroy(this TransferManager &) -> void;

    [[nodiscard]] auto alloc_transient_buffer_raw(
        this TransferManager &, vuk::MemoryUsage usage, usize size, vuk::source_location LOC = vuk::source_location::current())
        -> vuk::Buffer;
    [[nodiscard]] auto alloc_transient_buffer(
        this TransferManager &, vuk::MemoryUsage usage, usize size, vuk::source_location LOC = vuk::source_location::current())
        -> vuk::Value<vuk::Buffer>;
    [[nodiscard]] auto upload_staging(
        this TransferManager &,
        vuk::Value<vuk::Buffer> &&src,
        vuk::Value<vuk::Buffer> &&dst,
        vuk::source_location LOC = vuk::source_location::current()) -> vuk::Value<vuk::Buffer>;
    [[nodiscard]] auto upload_staging(
        this TransferManager &,
        vuk::Value<vuk::Buffer> &&src,
        Buffer &dst,
        u64 dst_offset = 0,
        vuk::source_location LOC = vuk::source_location::current()) -> vuk::Value<vuk::Buffer>;
    [[nodiscard]] auto upload_staging(
        this TransferManager &,
        ls::span<u8> bytes,
        vuk::Value<vuk::Buffer> &&dst,
        vuk::source_location LOC = vuk::source_location::current()) -> vuk::Value<vuk::Buffer>;
    [[nodiscard]] auto upload_staging(
        this TransferManager &,
        ls::span<u8> bytes,
        Buffer &dst,
        u64 dst_offset = 0,
        vuk::source_location LOC = vuk::source_location::current()) -> vuk::Value<vuk::Buffer>;
    [[nodiscard]] auto upload_staging(
        this TransferManager &, ImageView &image_view, ls::span<u8> bytes, vuk::source_location LOC = vuk::source_location::current())
        -> vuk::Value<vuk::ImageAttachment>;
    template<typename T>
    [[nodiscard]] auto upload_staging(
        this TransferManager &self,
        ls::span<T> span,
        Buffer &dst,
        u64 dst_offset = 0,
        vuk::source_location LOC = vuk::source_location::current()) -> vuk::Value<vuk::Buffer> {
        ZoneScoped;

        return self.upload_staging({ reinterpret_cast<u8 *>(span.data()), span.size_bytes() }, dst, dst_offset, LOC);
    }

    template<typename T>
    [[nodiscard]] auto upload_staging(
        this TransferManager &self,
        ls::span<T> span,
        vuk::Value<vuk::Buffer> &&dst,
        u64 dst_offset = 0,
        vuk::source_location LOC = vuk::source_location::current()) -> vuk::Value<vuk::Buffer> {
        ZoneScoped;

        return self.upload_staging({ reinterpret_cast<u8 *>(span.data()), span.size_bytes() }, std::move(dst), dst_offset, LOC);
    }

    template<typename T>
    [[nodiscard]] auto scratch_buffer(const T &val = {}, vuk::source_location LOC = vuk::source_location::current())
        -> vuk::Value<vuk::Buffer> {
        ZoneScoped;

        return scratch_buffer(&val, sizeof(T), LOC);
    }

    auto wait_on(this TransferManager &, vuk::UntypedValue &&fut) -> void;

    // This is host and device blocking. Please don't spam it
    auto resize_buffer(this TransferManager &, Buffer &buffer, u64 new_size) -> Buffer;

protected:
    [[nodiscard]] auto scratch_buffer(
        this TransferManager &, const void *data, u64 size, vuk::source_location LOC = vuk::source_location::current())
        -> vuk::Value<vuk::Buffer>;
    auto wait_for_ops(this TransferManager &, vuk::Allocator &allocator, vuk::Compiler &compiler) -> void;

    auto acquire(this TransferManager &, vuk::DeviceFrameResource &allocator) -> void;
    auto release(this TransferManager &) -> void;
};

struct DeviceResources {
    SlotMap<vuk::Unique<vuk::Buffer>, BufferID> buffers = {};
    SlotMap<vuk::Unique<vuk::Image>, ImageID> images = {};
    SlotMap<vuk::Unique<vuk::ImageView>, ImageViewID> image_views = {};
    SlotMap<vuk::Sampler, SamplerID> samplers = {};
    SlotMap<vuk::PipelineBaseInfo *, PipelineID> pipelines = {};
};

struct Device {
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

public:
    auto init(this Device &, usize frame_count) -> std::expected<void, vuk::VkException>;
    auto destroy(this Device &) -> void;

    auto new_slang_session(this Device &, const SlangSessionInfo &info) -> ls::option<SlangSession>;

    auto transfer_man(this Device &) -> TransferManager &;
    auto new_frame(this Device &, SwapChain &) -> vuk::Value<vuk::ImageAttachment>;
    auto end_frame(this Device &, vuk::Value<vuk::ImageAttachment> &&target_attachment) -> void;
    auto wait(this Device &, std::source_location LOC = std::source_location::current()) -> void;

    auto create_persistent_descriptor_set(this Device &, ls::span<BindlessDescriptorInfo> bindings, u32 index)
        -> vuk::Unique<vuk::PersistentDescriptorSet>;
    auto commit_descriptor_set(this Device &, vuk::PersistentDescriptorSet &set) -> void;

    auto set_name(this Device &, Buffer &buffer, std::string_view name) -> void;
    auto set_name(this Device &, Image &image, std::string_view name) -> void;
    auto set_name(this Device &, ImageView &image_view, std::string_view name) -> void;

    auto frame_count(this const Device &) -> usize;
    auto buffer(this Device &, BufferID) -> vuk::Buffer *;
    auto image(this Device &, ImageID) -> vuk::Image *;
    auto image_view(this Device &, ImageViewID) -> vuk::ImageView *;
    auto sampler(this Device &, SamplerID) -> vuk::Sampler *;
    auto pipeline(this Device &, PipelineID) -> vuk::PipelineBaseInfo **;

    auto destroy(this Device &, BufferID) -> void;
    auto destroy(this Device &, ImageID) -> void;
    auto destroy(this Device &, ImageViewID) -> void;
    auto destroy(this Device &, SamplerID) -> void;
    auto destroy(this Device &, PipelineID) -> void;

    struct Limits {
        constexpr static u32 FrameCount = 3;
    };

    auto get_instance() -> VkInstance { return instance.instance; }
};
}  // namespace lr
