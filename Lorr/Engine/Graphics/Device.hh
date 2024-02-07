#pragma once

#include "APIObject.hh"
#include "CommandList.hh"
#include "Descriptor.hh"
#include "MemoryAllocator.hh"
#include "Pipeline.hh"
#include "Resource.hh"
#include "SwapChain.hh"
#include "TracyVK.hh"

namespace lr::Graphics {
struct SubmitDesc {
    eastl::span<SemaphoreSubmitDesc> m_wait_semas = {};
    eastl::span<CommandListSubmitDesc> m_lists = {};
    eastl::span<SemaphoreSubmitDesc> m_signal_semas = {};
};

struct Device {
    void init(VkDevice handle, VkPhysicalDevice physical_device, VkInstance instance, eastl::span<u32> queue_indexes);

    /// COMMAND ///
    /// @returns - Success: APIResult::Success
    /// @returns - Failure: APIResult::[OutOfHostMem, OutOfDeviceMem]
    APIResult create_command_queue(CommandQueue *queue, CommandType type);
    CommandQueue *get_queue(CommandType type);
    u32 get_queue_index(CommandType type);
    /// @returns - Success: APIResult::Success
    /// @returns - Failure: APIResult::[OutOfHostMem, OutOfDeviceMem]
    APIResult create_command_allocator(CommandAllocator *allocator, CommandType type, CommandAllocatorFlag flags);
    void delete_command_allocator(CommandAllocator *allocator);
    /// @returns - Success: APIResult::Success
    /// @returns - Failure: APIResult::[OutOfHostMem, OutOfDeviceMem]
    APIResult create_command_list(CommandList *list, CommandAllocator *command_allocator);
    void delete_command_list(CommandList *list, CommandAllocator *command_allocator);

    void begin_command_list(CommandList *list);
    void end_command_list(CommandList *list);
    void reset_command_allocator(CommandAllocator *allocator);
    void submit(CommandQueue *queue, SubmitDesc *desc);

    /// @returns - Success: APIResult::Success
    /// @returns - Failure: APIResult::[OutOfHostMem, OutOfDeviceMem]
    APIResult create_binary_semaphore(Semaphore *semaphore);
    /// @returns - Success: APIResult::Success
    /// @returns - Failure: APIResult::[OutOfHostMem, OutOfDeviceMem]
    APIResult create_timeline_semaphore(Semaphore *semaphore, u64 initial_value);
    void delete_semaphore(Semaphore *semaphore);
    /// @returns - Success: APIResult::Success
    /// @returns - Failure: APIResult::[OutOfHostMem, OutOfDeviceMem, DeviceLost]
    APIResult wait_for_semaphore(Semaphore *semaphore, u64 desired_value, u64 timeout = UINT64_MAX);

    /// SWAPCHAIN ///
    /// @returns - Success: APIResult::Success
    /// @returns - Failure: APIResult::[OutOfHostMem, OutOfDeviceMem, DeviceLost, SurfaceLost, WindowInUse, InitFailed]
    APIResult create_swap_chain(SwapChain *swap_chain, SwapChainDesc *desc);
    void delete_swap_chain(SwapChain *swap_chain);

    void wait_for_work();
    /// @returns - Success: APIResult::[Success, Timeout, NotReady, Suboptimal]
    /// @returns - Failure: APIResult::[OutOfHostMem, OutOfDeviceMem, DeviceLost, OutOfDate, SurfaceLost]
    APIResult acquire_next_image(SwapChain *swap_chain, u32 &image_id);
    /// @returns - Success: APIResult::[Success, Timeout, Suboptimal]
    /// @returns - Failure: APIResult::[OutOfHostMem, OutOfDeviceMem, DeviceLost, OutOfDate, SurfaceLost]
    APIResult present(SwapChain *swap_chain, CommandQueue *queue);

    /// PIPELINE ///
    APIResult create_pipeline_layout(PipelineLayout *layout, eastl::span<DescriptorSetLayout> layouts, eastl::span<PushConstantDesc> push_constants);
    /// @returns - Success: APIResult::Success
    /// @returns - Failure: APIResult::[OutOfHostMem, OutOfDeviceMem]
    APIResult create_graphics_pipeline(Pipeline *pipeline, GraphicsPipelineInfo *pipeline_info, PipelineAttachmentInfo *pipeline_attachment_info);
    /// @returns - Success: APIResult::Success
    /// @returns - Failure: APIResult::[OutOfHostMem, OutOfDeviceMem]
    APIResult create_compute_pipeline(Pipeline *pipeline, ComputePipelineInfo *pipeline_info);
    void delete_pipeline(Pipeline *pipeline);

    // * Shaders * //
    /// @returns - Success: APIResult::Success
    /// @returns - Failure: APIResult::[OutOfHostMem, OutOfDeviceMem]
    APIResult create_shader(Shader *shader, ShaderStage stage, eastl::span<u32> ir);
    void delete_shader(Shader *shader);

    // * Descriptor * //
    APIResult create_descriptor_set_layout(DescriptorSetLayout *layout, eastl::span<DescriptorLayoutElement> elements, DescriptorSetLayoutFlag flags);
    void delete_descriptor_set_layout(DescriptorSetLayout *layout);
    // DESCRIPTOR BUFFER
    u64 get_descriptor_buffer_alignment();
    u64 get_descriptor_size(DescriptorType type);
    u64 get_descriptor_set_layout_size(DescriptorSetLayout *layout);
    u64 get_descriptor_set_layout_binding_offset(DescriptorSetLayout *layout, u32 binding_id);
    void get_descriptor_data(const DescriptorGetInfo &info, u64 data_size, void *data_out);
    // DESCRIPTOR POOL
    // For compat. reasons, i want to keep the legacy descriptor pools
    /// @returns - Success: APIResult::Success
    /// @returns - Failure: APIResult::[OutOfHostMem, OutOfDeviceMem]
    APIResult create_descriptor_pool(
        DescriptorPool *descriptor_pool,
        u32 max_sets,
        eastl::span<DescriptorPoolSize> pool_sizes,
        DescriptorPoolFlag flags = DescriptorPoolFlag::None);
    void delete_descriptor_pool(DescriptorPool *descriptor_pool);
    /// @returns - Success: APIResult::Success
    /// @returns - Failure: APIResult::[OutOfHostMem, OutOfDeviceMem, FragmentedPool, OutOfPoolMem]
    APIResult create_descriptor_set(DescriptorSet *descriptor_set, DescriptorSetLayout *layout, DescriptorPool *descriptor_pool);
    void delete_descriptor_set(DescriptorSet *descriptor_set, DescriptorPool *descriptor_pool);
    void update_descriptor_set(eastl::span<WriteDescriptorSet> writes, eastl::span<CopyDescriptorSet> copies);

    /// RESOURCE ///
    eastl::tuple<u64, u64> get_buffer_memory_size(BufferID buffer_id);
    eastl::tuple<u64, u64> get_image_memory_size(ImageID image_id);
    eastl::tuple<u64, u64> get_buffer_memory_size(Buffer *buffer);
    eastl::tuple<u64, u64> get_image_memory_size(Image *image);

    /// @returns - Success: APIResult::Success
    /// @returns - Failure: APIResult::[OutOfHostMem, OutOfDeviceMem, InvalidExternalHandle]
    APIResult create_device_memory(DeviceMemory *device_memory, DeviceMemoryDesc *desc);
    void delete_device_memory(DeviceMemory *device_memory);
    u32 get_memory_type_index(MemoryFlag flags);
    u32 get_heap_index(MemoryFlag flags);
    u64 get_heap_budget(MemoryFlag flags);
    void bind_memory(DeviceMemory *device_memory, BufferID buffer_id, u64 memory_offset);
    void bind_memory(DeviceMemory *device_memory, ImageID image_id, u64 memory_offset);
    void bind_memory_ex(DeviceMemory *device_memory, Buffer *buffer, u64 memory_offset);
    // * Buffers * //
    u64 get_buffer_alignment(BufferUsage usage, u64 size);
    /// @returns - Success: APIResult::Success
    /// @returns - Failure: APIResult::[OutOfHostMem, OutOfDeviceMem]
    BufferID create_buffer(BufferDesc *desc);
    void delete_buffer(BufferID buffer_id);
    Buffer *get_buffer(BufferID buffer_id);
    // Same as `create_buffer`, handle is externally handled.
    // good for transient buffer allocations.
    APIResult create_buffer_ex(Buffer &buffer, BufferDesc *desc);
    void delete_buffer_ex(Buffer &buffer);

    // * Images * //
    /// @returns - Success: APIResult::Success
    /// @returns - Failure: APIResult::[OutOfHostMem, OutOfDeviceMem]
    ImageID create_image(ImageDesc *desc);
    void delete_image(ImageID image_id);
    Image *get_image(ImageID image_id);

    /// @returns - Success: APIResult::Success
    /// @returns - Failure: APIResult::[OutOfHostMem, OutOfDeviceMem]
    ImageViewID create_image_view(ImageViewDesc *desc);
    void delete_image_view(ImageViewID image_view_id);
    ImageView *get_image_view(ImageViewID image_view_id);

    /// @returns - Success: APIResult::Success
    /// @returns - Failure: APIResult::[OutOfHostMem, OutOfDeviceMem]
    SamplerID create_sampler(SamplerDesc *desc);
    void delete_sampler(SamplerID sampler_id);
    Sampler *get_sampler(SamplerID sampler_id);

    /// UTILITY ///
    bool is_feature_supported(DeviceFeature feature) { return m_supported_features & feature; }

    template<typename TypeT>
    void set_object_name(TypeT *obj, eastl::string_view name)
    {
#if _DEBUG
        VkDebugUtilsObjectNameInfoEXT object_name_info = {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
            .objectType = (VkObjectType)ToVKObjectType<TypeT>::type,
            .objectHandle = (u64)obj->m_handle,
            .pObjectName = name.data(),
        };
        vkSetDebugUtilsObjectNameEXT(m_handle, &object_name_info);
#endif
    }

    template<VkObjectType TObjectType, typename TypeT>
    void set_object_name_raw(TypeT object, eastl::string_view name)
    {
#if _DEBUG
        VkDebugUtilsObjectNameInfoEXT object_name_info = {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
            .objectType = TObjectType,
            .objectHandle = (u64)object,
            .pObjectName = name.data(),
        };
        vkSetDebugUtilsObjectNameEXT(m_handle, &object_name_info);
#endif
    }

    eastl::array<CommandQueue, static_cast<usize>(CommandType::Count)> m_queues = {};
    eastl::array<u32, static_cast<usize>(CommandType::Count)> m_queue_indexes = {};
    DeviceFeature m_supported_features = {};
    ResourcePools m_resources = {};

    TracyVkCtx m_tracy_ctx = nullptr;
    VkPhysicalDevice m_physical_device = nullptr;
    VkInstance m_instance = nullptr;

    VkDevice m_handle = VK_NULL_HANDLE;
};

}  // namespace lr::Graphics
