#pragma once

#include "CommandList.hh"
#include "Descriptor.hh"
#include "Pipeline.hh"
#include "Resource.hh"
#include "ResourcePool.hh"
#include "SwapChain.hh"

namespace lr {
struct StagingAllocResult {
    BufferID buffer_id = BufferID::Invalid;
    u8 *ptr = nullptr;
    u64 offset = 0;
    u64 size = 0;
};

struct StagingBuffer {
    // Config
    constexpr static usize BLOCK_SIZE = mib_to_bytes(16);

    struct StagingBufferBlock {
        BufferID buffer_id = BufferID::Invalid;
        u64 offset = 0;
    };

    plf::colony<StagingBufferBlock> blocks = {};
    Device *device = nullptr;

    void init(this StagingBuffer &, Device *device);
    StagingAllocResult alloc(this StagingBuffer &, usize size, u64 alignment = 8);
    void reset(this StagingBuffer &);
    u64 size(this StagingBuffer &);
    void upload(this StagingBuffer &, StagingAllocResult &result, BufferID dst_buffer, CommandList &cmd_list);
};

struct DeviceInfo {
    vkb::Instance *instance = nullptr;
    usize frame_count = 3;
};

struct Device {
    constexpr static usize QUEUE_COUNT = static_cast<usize>(CommandType::Count);
    std::array<CommandQueue, QUEUE_COUNT> queues = {};
    ls::static_vector<StagingBuffer, Limits::FrameCount> staging_buffers = {};
    Semaphore frame_sema = {};
    usize frame_count = 0;  // global frame count, same across all swap chains

    DescriptorPool descriptor_pool = {};
    DescriptorSetLayout descriptor_set_layout = {};
    DescriptorSet descriptor_set = {};

    BufferID bda_array_buffer = BufferID::Invalid;
    u64 *bda_array_host_addr = nullptr;

    DeviceFeature supported_features = DeviceFeature::None;
    ResourcePools resources = {};
    VmaAllocator allocator = {};
    vkb::PhysicalDevice physical_device = {};
    vkb::Instance *instance = nullptr;
    vkb::Device handle = {};

    VKResult init(this Device &, const DeviceInfo &info);

    /// Commands ///
    VKResult create_timestamp_query_pools(this Device &, ls::span<TimestampQueryPool> query_pools, const TimestampQueryPoolInfo &info);
    void delete_timestamp_query_pools(this Device &, ls::span<TimestampQueryPool> query_pools);

    void get_timestamp_query_pool_results(this Device &, TimestampQueryPool &query_pool, u32 first_query, u32 count, ls::span<u64> time_stamps);

    VKResult create_command_lists(this Device &, ls::span<CommandList> command_lists, CommandAllocator &allocator);
    void delete_command_lists(this Device &, ls::span<CommandList> command_lists);

    void reset_command_allocator(this Device &, CommandAllocator &allocator);

    VKResult create_binary_semaphores(this Device &, ls::span<Semaphore> semaphores);
    VKResult create_timeline_semaphores(this Device &, ls::span<Semaphore> semaphores, u64 initial_value);
    void delete_semaphores(this Device &, ls::span<Semaphore> semaphores);

    VKResult wait_for_semaphore(this Device &, Semaphore &semaphore, u64 desired_value, u64 timeout = UINT64_MAX);
    Result<u64, VKResult> get_semaphore_counter(this Device &, Semaphore &semaphore);

    /// Presentation ///
    VKResult create_swap_chain(this Device &, SwapChain &swap_chain, const SwapChainInfo &info);
    void delete_swap_chains(this Device &, ls::span<SwapChain> swap_chain);
    VKResult get_swapchain_images(this Device &, SwapChain &swap_chain, ls::span<ImageID> images);

    void wait_for_work(this Device &);
    usize new_frame(this Device &);
    Result<u32, VKResult> acquire_next_image(this Device &, SwapChain &swap_chain, Semaphore &acquire_sema);
    void end_frame(this Device &);

    /// Input Assembly ///
    VKResult create_pipeline_layouts(this Device &, ls::span<PipelineLayout> pipeline_layouts, const PipelineLayoutInfo &info);
    void delete_pipeline_layouts(this Device &, ls::span<PipelineLayout> pipeline_layouts);
    Result<PipelineID, VKResult> create_graphics_pipeline(this Device &, const GraphicsPipelineInfo &info);
    Result<PipelineID, VKResult> create_compute_pipeline(this Device &, const ComputePipelineInfo &info);
    void delete_pipelines(this Device &, ls::span<PipelineID> pipelines);

    /// Shaders ///
    Result<ShaderID, VKResult> create_shader(this Device &, ShaderStageFlag stage, ls::span<u32> ir);
    void delete_shaders(this Device &, ls::span<ShaderID> shaders);

    /// Descriptor ///
    VKResult create_descriptor_pools(this Device &, ls::span<DescriptorPool> descriptor_pools, const DescriptorPoolInfo &info);
    void delete_descriptor_pools(this Device &, ls::span<DescriptorPool> descriptor_pools);

    VKResult create_descriptor_set_layouts(this Device &, ls::span<DescriptorSetLayout> descriptor_set_layouts, const DescriptorSetLayoutInfo &info);
    void delete_descriptor_set_layouts(this Device &, ls::span<DescriptorSetLayout> layouts);

    VKResult create_descriptor_sets(this Device &, ls::span<DescriptorSet> descriptor_sets, const DescriptorSetInfo &info);
    void delete_descriptor_sets(this Device &, ls::span<DescriptorSet> descriptor_sets);
    void update_descriptor_sets(this Device &, ls::span<WriteDescriptorSet> writes, ls::span<CopyDescriptorSet> copies);

    /// Buffers ///
    Result<BufferID, VKResult> create_buffer(this Device &, const BufferInfo &info);
    void delete_buffers(this Device &, ls::span<BufferID> buffers);
    template<typename T>
    T *buffer_host_data(this Device &, BufferID buffer_id);

    MemoryRequirements memory_requirements(this Device &, BufferID buffer_id);

    /// Images ///
    Result<ImageID, VKResult> create_image(this Device &, const ImageInfo &info);
    void delete_images(this Device &, ls::span<ImageID> images);

    MemoryRequirements memory_requirements(this Device &, ImageID image_id);

    Result<ImageViewID, VKResult> create_image_view(this Device &, const ImageViewInfo &info);
    void delete_image_views(this Device &, ls::span<ImageViewID> image_views);

    Result<SamplerID, VKResult> create_sampler(this Device &, const SamplerInfo &info);
    void delete_samplers(this Device &, ls::span<SamplerID> samplers);

    Result<SamplerID, VKResult> create_cached_sampler(this Device &, const SamplerInfo &info);

    void set_image_data(this Device &, ImageID image_id, const void *data, ImageLayout new_layout, ImageLayout old_layout = ImageLayout::Undefined);

    /// Utils ///
    bool is_feature_supported(this auto &self, DeviceFeature feature) { return self.supported_features & feature; }

    template<typename T>
    void set_object_name(this auto &self, [[maybe_unused]] T &v, [[maybe_unused]] std::string_view name)
    {
#if LR_DEBUG
        VkDebugUtilsObjectNameInfoEXT object_name_info = {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
            .pNext = nullptr,
            .objectType = T::OBJECT_TYPE,
            .objectHandle = (u64)v.handle,
            .pObjectName = name.data(),
        };
        vkSetDebugUtilsObjectNameEXT(self, &object_name_info);
#endif
    }

    template<VkObjectType ObjectType, typename T>
    void set_object_name_raw(this auto &self, [[maybe_unused]] T v, [[maybe_unused]] std::string_view name)
    {
#if LR_DEBUG
        VkDebugUtilsObjectNameInfoEXT object_name_info = {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
            .pNext = nullptr,
            .objectType = ObjectType,
            .objectHandle = (u64)v,
            .pObjectName = name.data(),
        };
        vkSetDebugUtilsObjectNameEXT(self, &object_name_info);
#endif
    }

    template<typename T = void>
    PipelineLayoutID get_pipeline_layout()
    {
        if constexpr (std::is_same_v<T, void>) {
            return static_cast<PipelineLayoutID>(0);
        }
        else {
            usize index = sizeof(T) / sizeof(u32);
            return static_cast<PipelineLayoutID>(index);
        }
    }

    auto queue_at(this auto &self, CommandType type) -> CommandQueue & { return self.queues[usize(type)]; }
    auto image_at(this auto &self, ImageID id) -> Image & { return self.resources.images.get(id); }
    auto image_view_at(this auto &self, ImageViewID id) -> ImageView & { return self.resources.image_views.get(id); }
    auto sampler_at(this auto &self, SamplerID id) -> Sampler & { return self.resources.samplers.get(id); }
    auto buffer_at(this auto &self, BufferID id) -> Buffer & { return self.resources.buffers.get(id); }
    auto pipeline_at(this auto &self, PipelineID id) -> Pipeline & { return self.resources.pipelines.get(id); }
    auto shader_at(this auto &self, ShaderID id) -> Shader & { return self.resources.shaders.get(id); }
    auto pipeline_layout_at(this auto &self, PipelineLayoutID id) -> PipelineLayout & { return self.resources.pipeline_layouts[static_cast<usize>(id)]; }
    StagingBuffer &staging_buffer_at(this auto &self, usize i) { return self.staging_buffers[i]; }

    usize sema_index(this auto &self) { return self.frame_sema.counter % self.frame_count; }

    constexpr static auto OBJECT_TYPE = VK_OBJECT_TYPE_DEVICE;
    operator auto &(this Device & self) { return self.handle.device; }
    explicit operator bool(this Device &self) { return self.handle.device != nullptr; }

private:
    VKResult create_command_queue(this Device &, CommandQueue &command_queue, CommandType type, std::string_view name = {});
    VKResult create_command_allocators(this Device &, ls::span<CommandAllocator> command_allocators, const CommandAllocatorInfo &info);
};

template<typename T = u8>
T *Device::buffer_host_data(this Device &self, BufferID buffer_id)
{
    return reinterpret_cast<T *>(self.buffer_at(buffer_id).host_data);
}
}  // namespace lr
