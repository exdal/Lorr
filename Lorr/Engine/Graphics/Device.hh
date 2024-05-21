#pragma once

#include "CommandList.hh"
#include "Descriptor.hh"
#include "Pipeline.hh"
#include "Resource.hh"
#include "ResourcePool.hh"
#include "SwapChain.hh"

namespace lr::graphics {
struct Device {
    Device() = default;
    VKResult init(vkb::Instance *instance);

    /// Commands ///
    VKResult create_timestamp_query_pools(std::span<TimestampQueryPool> query_pools, const TimestampQueryPoolInfo &info);
    void delete_timestamp_query_pools(std::span<const TimestampQueryPool> query_pools);
    void defer(std::span<const TimestampQueryPool> query_pools);

    void get_timestamp_query_pool_results(TimestampQueryPool &query_pool, u32 first_query, u32 count, std::span<u64> time_stamps);

    VKResult create_command_allocators(std::span<CommandAllocator> command_allocators, const CommandAllocatorInfo &info);
    void delete_command_allocators(std::span<const CommandAllocator> command_allocators);
    void defer(std::span<const CommandAllocator> command_allocators);

    VKResult create_command_lists(std::span<CommandList> command_lists, CommandAllocator &command_allocator);
    void delete_command_lists(std::span<const CommandList> command_lists);
    void defer(std::span<const CommandList> command_lists);

    void begin_command_list(CommandList &list);
    void end_command_list(CommandList &list);
    void reset_command_allocator(CommandAllocator &allocator);

    VKResult create_binary_semaphores(std::span<Semaphore> semaphores);
    VKResult create_timeline_semaphores(std::span<Semaphore> semaphores, u64 initial_value);
    void delete_semaphores(std::span<const Semaphore> semaphores);
    void defer(std::span<const Semaphore> semaphores);

    VKResult wait_for_semaphore(Semaphore &semaphore, u64 desired_value, u64 timeout = UINT64_MAX);
    Result<u64, VKResult> get_semaphore_counter(Semaphore &semaphore);

    /// Presentation ///
    VKResult create_swap_chain(SwapChain &swap_chain, const SwapChainInfo &info);
    void delete_swap_chains(std::span<const SwapChain> swap_chain);
    VKResult get_swapchain_images(SwapChain &swap_chain, std::span<ImageID> images);

    void wait_for_work();
    Result<u32, VKResult> acquire_next_image(SwapChain &swap_chain, Semaphore &acquire_sema);
    void collect_garbage();

    /// Input Assembly ///
    VKResult create_pipeline_layouts(std::span<PipelineLayout> pipeline_layouts, const PipelineLayoutInfo &info);
    void delete_pipeline_layouts(std::span<const PipelineLayout> pipeline_layouts);
    Result<PipelineID, VKResult> create_graphics_pipeline(const GraphicsPipelineInfo &info);
    Result<PipelineID, VKResult> create_compute_pipeline(const ComputePipelineInfo &info);
    void delete_pipelines(std::span<const PipelineID> pipelines);

    /// Shaders ///
    Result<ShaderID, VKResult> create_shader(ShaderStageFlag stage, std::span<u32> ir);
    void delete_shaders(std::span<const ShaderID> shaders);

    /// Descriptor ///
    VKResult create_descriptor_pools(std::span<DescriptorPool> descriptor_pools, const DescriptorPoolInfo &info);
    void delete_descriptor_pools(std::span<const DescriptorPool> descriptor_pools);

    VKResult create_descriptor_set_layouts(std::span<DescriptorSetLayout> descriptor_set_layouts, const DescriptorSetLayoutInfo &info);
    void delete_descriptor_set_layouts(std::span<const DescriptorSetLayout> layouts);

    VKResult create_descriptor_sets(std::span<DescriptorSet> descriptor_sets, const DescriptorSetInfo &info);
    void delete_descriptor_sets(std::span<const DescriptorSet> descriptor_sets);
    void update_descriptor_sets(std::span<const WriteDescriptorSet> writes, std::span<const CopyDescriptorSet> copies);

    /// Buffers ///
    Result<BufferID, VKResult> create_buffer(const BufferInfo &info);
    void delete_buffers(std::span<const BufferID> buffers);
    template<typename T>
    T *buffer_host_data(BufferID buffer_id);

    /// Images ///
    Result<ImageID, VKResult> create_image(const ImageInfo &info);
    void delete_images(std::span<const ImageID> images);

    Result<ImageViewID, VKResult> create_image_view(const ImageViewInfo &info);
    void delete_image_views(std::span<const ImageViewID> image_views);

    Result<SamplerID, VKResult> create_sampler(const SamplerInfo &info);
    void delete_samplers(std::span<const SamplerID> samplers);

    /// Utils ///
    bool is_feature_supported(DeviceFeature feature) { return m_supported_features & feature; }

    template<typename T>
    void set_object_name([[maybe_unused]] T &v, [[maybe_unused]] std::string_view name)
    {
#if LR_DEBUG
        VkDebugUtilsObjectNameInfoEXT object_name_info = {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
            .pNext = nullptr,
            .objectType = T::OBJECT_TYPE,
            .objectHandle = (u64)v.m_handle,
            .pObjectName = name.data(),
        };
        vkSetDebugUtilsObjectNameEXT(m_handle, &object_name_info);
#endif
    }

    template<VkObjectType ObjectType, typename T>
    void set_object_name_raw([[maybe_unused]] T v, [[maybe_unused]] std::string_view name)
    {
#if LR_DEBUG
        VkDebugUtilsObjectNameInfoEXT object_name_info = {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
            .pNext = nullptr,
            .objectType = ObjectType,
            .objectHandle = (u64)v,
            .pObjectName = name.data(),
        };
        vkSetDebugUtilsObjectNameEXT(m_handle, &object_name_info);
#endif
    }

    template<typename T = void>
    PipelineLayout *get_layout()
    {
        if constexpr (std::is_same_v<T, void>) {
            return &m_resources.pipeline_layouts[0];
        }
        else {
            usize index = sizeof(T) / sizeof(u32);
            return &m_resources.pipeline_layouts[index];
        }
    }
    CommandQueue &get_queue(CommandType type) { return m_queues[usize(type)]; }
    auto get_image(ImageID id) { return m_resources.images.get(id); }
    auto get_image_view(ImageViewID id) { return m_resources.image_views.get(id); }
    auto get_sampler(SamplerID id) { return m_resources.samplers.get(id); }
    auto get_buffer(BufferID id) { return m_resources.buffers.get(id); }
    auto get_pipeline(PipelineID id) { return m_resources.pipelines.get(id); }
    auto get_shader(ShaderID id) { return m_resources.shaders.get(id); }

    Semaphore m_garbage_timeline_sema = {};
    plf::colony<std::pair<TimestampQueryPool, u64>> m_garbage_query_pools = {};
    plf::colony<std::pair<Semaphore, u64>> m_garbage_semaphores = {};

    std::array<CommandQueue, usize(CommandType::Count)> m_queues = {};
    DescriptorPool m_descriptor_pool = {};
    DescriptorSetLayout m_descriptor_set_layout = {};
    DescriptorSet m_descriptor_set = {};

    BufferID m_bda_array_buffer = BufferID::Invalid;
    u64 *m_bda_array_host_addr = nullptr;

    DeviceFeature m_supported_features = DeviceFeature::None;

    ResourcePools m_resources = {};
    VmaAllocator m_allocator = {};
    vkb::PhysicalDevice m_physical_device = {};
    vkb::Instance *m_instance = nullptr;
    vkb::Device m_handle = {};

    constexpr static auto OBJECT_TYPE = VK_OBJECT_TYPE_DEVICE;
    operator auto &() { return m_handle.device; }
    explicit operator bool() { return m_handle != nullptr; }
};

template<typename T>
T *Device::buffer_host_data(BufferID buffer_id)
{
    return reinterpret_cast<T *>(get_buffer(buffer_id)->m_host_data);
}

namespace unique_impl {
    template<class T>
    concept is_container = requires(T v) {
        std::begin(v);
        std::end(v);
    };

    template<typename T, usize N>
    void device_defer_helper(Device *device, T (&arr)[N])
    {
        device->defer(std::span<T>{ arr, N });
    }

    template<typename T>
    void device_defer_helper(Device *device, T &v)
        requires(!is_container<T>)
    {
        device->defer(std::span<T>{ &v, 1 });
    }

    template<typename T>
    void device_defer_helper(Device *device, T &v)
        requires(is_container<T>)
    {
        device->defer(v);
    }
}  // namespace unique_impl

template<typename T>
void Unique<T>::set_name(std::string_view name)
{
    m_device->set_object_name(m_val, name);
}

template<typename T>
void Unique<T>::reset(Unique<T>::val_type val) noexcept
{
    if (m_val == val) {
        return;
    }

    if (m_device && m_val != val_type{}) {
        unique_impl::device_defer_helper(m_device, m_val);
    }

    m_val = std::move(val);
}
}  // namespace lr::graphics
