#pragma once

#include "CommandList.hh"
#include "Descriptor.hh"
#include "Pipeline.hh"
#include "Resource.hh"
#include "ResourcePool.hh"
#include "SwapChain.hh"

#include <deque>

namespace lr::graphics {
struct Device {
    Device() = default;
    VKResult init(vkb::Instance *instance);

    /// COMMAND ///
    VKResult create_command_allocators(std::span<CommandAllocator> allocators, const CommandAllocatorInfo &info);
    void delete_command_allocators(std::span<CommandAllocator> allocators);
    VKResult create_command_lists(std::span<CommandList> lists, CommandAllocator &command_allocator);
    void delete_command_lists(std::span<CommandList> lists);

    void begin_command_list(CommandList &list);
    void end_command_list(CommandList &list);
    void reset_command_allocator(CommandAllocator &allocator);
    void submit(CommandType queue_type, QueueSubmitInfo &submit_info);

    VKResult create_binary_semaphores(std::span<Semaphore> semaphores);
    VKResult create_timeline_semaphores(std::span<Semaphore> semaphores, u64 initial_value);
    void delete_semaphores(std::span<Semaphore> semaphores);
    VKResult wait_for_semaphore(Semaphore &semaphore, u64 desired_value, u64 timeout = UINT64_MAX);
    Result<u64, VKResult> get_semaphore_counter(Semaphore &semaphore);

    /// Presentation ///
    VKResult create_swap_chain(SwapChain &swap_chain, const SwapChainInfo &info);
    void delete_swap_chains(std::span<SwapChain> swap_chain);
    VKResult get_swapchain_images(SwapChain &swap_chain, std::span<ImageID> images);

    void wait_for_work();
    Result<u32, VKResult> acquire_next_image(SwapChain &swap_chain, Semaphore &acquire_sema);
    VKResult present(SwapChain &swap_chain, Semaphore &present_sema, u32 image_id);
    void collect_garbage();

    /// Input Assembly ///
    UniqueResult<PipelineLayout> create_pipeline_layout(const PipelineLayoutInfo &info);
    void delete_pipeline_layouts(std::span<PipelineLayout> pipeline_layouts);
    Result<PipelineID, VKResult> create_graphics_pipeline(const GraphicsPipelineInfo &info);
    Result<PipelineID, VKResult> create_compute_pipeline(const ComputePipelineInfo &info);
    void delete_pipelines(std::span<PipelineID> pipelines);

    /// Shaders ///
    Result<ShaderID, VKResult> create_shader(ShaderStageFlag stage, std::span<u32> ir);
    void delete_shaders(std::span<ShaderID> shaders);

    /// Descriptor ///
    UniqueResult<DescriptorPool> create_descriptor_pool(const DescriptorPoolInfo &info);
    void delete_descriptor_pools(std::span<DescriptorPool> descriptor_pools);

    UniqueResult<DescriptorSetLayout> create_descriptor_set_layout(const DescriptorSetLayoutInfo &info);
    void delete_descriptor_set_layouts(std::span<DescriptorSetLayout> layouts);

    UniqueResult<DescriptorSet> create_descriptor_set(const DescriptorSetInfo &info);
    void delete_descriptor_sets(std::span<DescriptorSet> descriptor_sets);
    void update_descriptor_set(std::span<WriteDescriptorSet> writes, std::span<CopyDescriptorSet> copies);

    /// Buffers ///
    Result<BufferID, VKResult> create_buffer(const BufferInfo &info);
    void delete_buffers(std::span<BufferID> buffers);

    /// Images ///
    Result<ImageID, VKResult> create_image(const ImageInfo &info);
    void delete_images(std::span<ImageID> images);

    Result<ImageViewID, VKResult> create_image_view(const ImageViewInfo &info);
    void delete_image_views(std::span<ImageViewID> image_views);

    Result<SamplerID, VKResult> create_sampler(const SamplerInfo &info);
    void delete_samplers(std::span<SamplerID> samplers);

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

    CommandQueue &get_queue(CommandType type) { return m_queues->at(usize(type)); }
    auto get_image(ImageID id) { return m_resources.images.get(id); }
    auto get_image_view(ImageViewID id) { return m_resources.image_views.get(id); }
    auto get_sampler(SamplerID id) { return m_resources.samplers.get(id); }
    auto get_buffer(BufferID id) { return m_resources.buffers.get(id); }
    auto get_pipeline(PipelineID id) { return m_resources.pipelines.get(id); }
    auto get_shader(ShaderID id) { return m_resources.shaders.get(id); }

    plf::colony<std::pair<CommandAllocator, u64>> m_garbage_command_allocators = {};
    plf::colony<std::pair<CommandList, u64>> m_garbage_command_lists = {};
    plf::colony<std::pair<Semaphore, u64>> m_garbage_semaphores = {};
    plf::colony<std::pair<ShaderID, u64>> m_garbage_shaders = {};
    plf::colony<std::pair<BufferID, u64>> m_garbage_buffers = {};
    plf::colony<std::pair<ImageID, u64>> m_garbage_images = {};
    plf::colony<std::pair<ImageViewID, u64>> m_garbage_image_views = {};
    plf::colony<std::pair<SamplerID, u64>> m_garbage_samplers = {};

    Unique<DescriptorPool> m_descriptor_pool;
    Unique<DescriptorSetLayout> m_descriptor_set_layout;
    Unique<DescriptorSet> m_descriptor_set;

    DeviceFeature m_supported_features = DeviceFeature::None;
    Unique<std::array<CommandQueue, usize(CommandType::Count)>> m_queues;

    ResourcePools m_resources = {};
    VmaAllocator m_allocator = {};
    vkb::PhysicalDevice m_physical_device = {};
    vkb::Instance *m_instance = nullptr;
    vkb::Device m_handle = {};

    constexpr static auto OBJECT_TYPE = VK_OBJECT_TYPE_DEVICE;
    operator auto &() { return m_handle.device; }
    explicit operator bool() { return m_handle != nullptr; }
};

struct DeviceDeleter {
    // clang-format off
    DeviceDeleter(Device *device, [[maybe_unused]] u64 c, std::span<CommandQueue> s) { for (auto &v : s) DeviceDeleter(device, c, { &v.m_semaphore, 1 }); }
    DeviceDeleter(Device *device, [[maybe_unused]] u64 c, std::span<CommandAllocator> s) { for (auto &v : s) device->m_garbage_command_allocators.emplace(v, c); }
    DeviceDeleter(Device *device, [[maybe_unused]] u64 c, std::span<CommandList> s) { for (auto &v : s) device->m_garbage_command_lists.emplace(v, c); }
    DeviceDeleter(Device *device, [[maybe_unused]] u64 c, std::span<Semaphore> s) { for (auto &v : s) device->m_garbage_semaphores.emplace(v, c); }
    DeviceDeleter(Device *device, [[maybe_unused]] u64 c, std::span<SwapChain> s) { device->delete_swap_chains(s); }
    DeviceDeleter(Device *device, [[maybe_unused]] u64 c, std::span<PipelineLayout> s) { device->delete_pipeline_layouts(s); }
    DeviceDeleter(Device *device, [[maybe_unused]] u64 c, std::span<PipelineID> s) { device->delete_pipelines(s); }
    DeviceDeleter(Device *device, [[maybe_unused]] u64 c, std::span<ShaderID> s) { for (auto &v : s) device->m_garbage_shaders.emplace(v, c); }
    DeviceDeleter(Device *device, [[maybe_unused]] u64 c, std::span<DescriptorPool> s) { device->delete_descriptor_pools(s); }
    DeviceDeleter(Device *device, [[maybe_unused]] u64 c, std::span<DescriptorSetLayout> s) { device->delete_descriptor_set_layouts(s); }
    DeviceDeleter(Device *device, [[maybe_unused]] u64 c, std::span<DescriptorSet> s) { device->delete_descriptor_sets(s); }
    DeviceDeleter(Device *device, [[maybe_unused]] u64 c, std::span<BufferID> s) { for (auto &v : s) device->m_garbage_buffers.emplace(v, c); }
    DeviceDeleter(Device *device, [[maybe_unused]] u64 c, std::span<ImageID> s) { for (auto &v : s) device->m_garbage_images.emplace(v, c); }
    DeviceDeleter(Device *device, [[maybe_unused]] u64 c, std::span<ImageViewID> s) { for (auto &v : s) device->m_garbage_image_views.emplace(v, c); }
    DeviceDeleter(Device *device, [[maybe_unused]] u64 c, std::span<SamplerID> s) { for (auto &v : s) device->m_garbage_samplers.emplace(v, c); }
    // clang-format on
};

template<class T>
concept is_container = requires(T v) {
    std::begin(v);
    std::end(v);
};

template<typename T, usize N>
void device_deleter_helper(Device *device, u64 c, T (&arr)[N])
{
    DeviceDeleter(device, c, std::span<T>{ arr, N });
}

template<typename T>
void device_deleter_helper(Device *device, u64 c, T &v)
    requires(!is_container<T>)
{
    DeviceDeleter(device, c, std::span<T>{ &v, 1 });
}

template<typename T>
void device_deleter_helper(Device *device, u64 c, T &v)
    requires(is_container<T>)
{
    DeviceDeleter(device, c, v);
}

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
        Semaphore &sema = m_device->get_queue(CommandType::Graphics).semaphore();
        device_deleter_helper(m_device, sema.counter(), m_val);
    }

    m_val = std::move(val);
}
}  // namespace lr::graphics
