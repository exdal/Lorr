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

#ifdef VUK_IA
    #undef VUK_IA
    #define VUK_IA(access) [[maybe_unused]] vuk::Arg<vuk::ImageAttachment, access, vuk::tag_type<__COUNTER__>>
#endif

#ifdef VUK_BA
    #undef VUK_BA
    #define VUK_BA(access) [[maybe_unused]] vuk::Arg<vuk::Buffer, access, vuk::tag_type<__COUNTER__>>
#endif

#ifdef VUK_ARG
    #undef VUK_ARG
    #define VUK_ARG(type, access) [[maybe_unused]] vuk::Arg<type, access, vuk::tag_type<__COUNTER__>>
#endif

namespace lr {
struct BindlessDescriptorInfo {
    u32 binding = 0;
    vuk::DescriptorType type = {};
    u32 descriptor_count = 0;
};

struct TransferManager {
private:
    Device *device = nullptr;

    mutable std::shared_mutex mutex = {};
    std::vector<vuk::UntypedValue> futures = {};

    ls::option<vuk::Allocator> frame_allocator;

    friend Device;

public:
    TransferManager &operator=(const TransferManager &) = delete;
    TransferManager &operator=(TransferManager &&) = delete;

    auto init(Device &) -> std::expected<void, vuk::VkException>;
    auto destroy(this TransferManager &) -> void;

    [[nodiscard]] auto
    alloc_transient_buffer_raw(this TransferManager &, vuk::MemoryUsage usage, usize size, usize alignment = 8, bool frame = true, LR_THISCALL)
        -> vuk::Buffer;
    [[nodiscard]] auto
    alloc_transient_buffer(this TransferManager &, vuk::MemoryUsage usage, usize size, usize alignment = 8, bool frame = true, LR_THISCALL)
        -> vuk::Value<vuk::Buffer>;
    [[nodiscard]] auto upload_staging(this TransferManager &, vuk::Value<vuk::Buffer> &&src, vuk::Value<vuk::Buffer> &&dst, LR_THISCALL)
        -> vuk::Value<vuk::Buffer>;
    [[nodiscard]] auto upload_staging(this TransferManager &, vuk::Value<vuk::Buffer> &&src, Buffer &dst, u64 dst_offset = 0, LR_THISCALL)
        -> vuk::Value<vuk::Buffer>;
    [[nodiscard]] auto
    upload_staging(this TransferManager &, void *data, u64 data_size, vuk::Value<vuk::Buffer> &&dst, u64 dst_offset = 0, LR_THISCALL)
        -> vuk::Value<vuk::Buffer>;
    [[nodiscard]] auto upload_staging(this TransferManager &, void *data, u64 data_size, Buffer &dst, u64 dst_offset = 0, LR_THISCALL)
        -> vuk::Value<vuk::Buffer>;
    [[nodiscard]] auto upload_staging(this TransferManager &, ImageView &image_view, void *data, u64 data_size, LR_THISCALL)
        -> vuk::Value<vuk::ImageAttachment>;
    template<typename T>
    [[nodiscard]] auto upload_staging(this TransferManager &self, ls::span<T> span, Buffer &dst, u64 dst_offset = 0, LR_THISCALL)
        -> vuk::Value<vuk::Buffer> {
        ZoneScoped;

        return self.upload_staging(reinterpret_cast<void *>(span.data()), span.size_bytes(), dst, dst_offset, LOC);
    }

    template<typename T>
    [[nodiscard]] auto upload_staging(this TransferManager &self, ls::span<T> span, vuk::Value<vuk::Buffer> &&dst, u64 dst_offset = 0, LR_THISCALL)
        -> vuk::Value<vuk::Buffer> {
        ZoneScoped;

        return self.upload_staging(reinterpret_cast<void *>(span.data()), span.size_bytes(), std::move(dst), dst_offset, LOC);
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

    auto acquire(this TransferManager &, vuk::DeviceFrameResource &allocator) -> void;
    auto release(this TransferManager &) -> void;
};

struct DeviceResources {
    SlotMap<vuk::Unique<vuk::Buffer>, BufferID> buffers = {};
    SlotMap<vuk::Unique<vuk::Image>, ImageID> images = {};
    SlotMap<vuk::Unique<vuk::ImageView>, ImageViewID> image_views = {};
    SlotMap<vuk::Sampler, SamplerID> samplers = {};
    SlotMap<vuk::PipelineBaseInfo *, PipelineID> pipelines = {};

    vuk::PersistentDescriptorSet bindless_set = {};
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
    ankerl::unordered_dense::map<vuk::Name, ls::pair<vuk::Query, vuk::Query>> pass_queries = {};
    std::vector<legit::ProfilerTask> gpu_profiler_tasks = {};
    legit::ProfilerGraph gpu_profiler_graph = { 400 };
    f64 gpu_profiler_query_offset = 0.0;

    vkb::Instance instance = {};
    vkb::PhysicalDevice physical_device = {};
    vkb::Device handle = {};

    friend Buffer;
    friend Image;
    friend ImageView;
    friend Sampler;
    friend Pipeline;
    friend TransferManager;

public:
    auto init(this Device &, usize frame_count) -> std::expected<void, vuk::VkException>;
    auto destroy(this Device &) -> void;

    auto new_slang_session(this Device &, const SlangSessionInfo &info) -> ls::option<SlangSession>;

    auto transfer_man(this Device &) -> TransferManager &;
    auto new_frame(this Device &, vuk::Swapchain &) -> vuk::Value<vuk::ImageAttachment>;
    auto end_frame(this Device &, vuk::Value<vuk::ImageAttachment> &&target_attachment) -> void;
    auto wait(this Device &, std::source_location LOC = std::source_location::current()) -> void;

    auto create_persistent_descriptor_set(this Device &, ls::span<BindlessDescriptorInfo> bindings, u32 index)
        -> vuk::Unique<vuk::PersistentDescriptorSet>;
    auto commit_descriptor_set(this Device &, vuk::PersistentDescriptorSet &set) -> void;
    auto create_swap_chain(this Device &, VkSurfaceKHR surface, ls::option<vuk::Swapchain> old_swap_chain = ls::nullopt)
        -> std::expected<vuk::Swapchain, vuk::VkException>;

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

    auto bindless_set(this Device &) -> vuk::PersistentDescriptorSet &;
    auto recreate_bindless_set(this Device &) -> void;
    auto refresh_bindless_set(this Device &, ImageViewID image_view_id, const vuk::ImageUsageFlags &usage) -> void;
    auto refresh_bindless_set(this Device &, SamplerID sampler_id) -> void;

    struct Limits {
        constexpr static u32 FrameCount = 3;
    };

    auto get_instance() -> VkInstance {
        return instance.instance;
    }
    auto get_allocator() -> vuk::Allocator & {
        return allocator.value();
    }

    auto render_frame_profiler(this Device &) -> void;
};
} // namespace lr
