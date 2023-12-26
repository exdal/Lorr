#pragma once

#include "STL/RefPtr.hh"

#include "APIObject.hh"
#include "Resource.hh"
#include "PhysicalDevice.hh"
#include "CommandList.hh"
#include "Pipeline.hh"
#include "SwapChain.hh"
#include "TracyVK.hh"

namespace lr::Graphics
{
struct SubmitDesc
{
    eastl::span<SemaphoreSubmitDesc> m_wait_semas;
    eastl::span<CommandListSubmitDesc> m_lists;
    eastl::span<SemaphoreSubmitDesc> m_signal_semas;
};

struct Device : Tracked<VkDevice>
{
    void init(VkDevice handle, PhysicalDevice *physical_device);

    /// COMMAND ///
    /// @returns - Success: APIResult::Success
    /// @returns - Failure: APIResult::[OutOfHostMem, OutOfDeviceMem]
    APIResult create_command_queue(CommandQueue *queue, CommandTypeMask type_mask);
    CommandQueue *get_queue(CommandType type);
    /// @returns - Success: APIResult::Success
    /// @returns - Failure: APIResult::[OutOfHostMem, OutOfDeviceMem]
    APIResult create_command_allocator(CommandAllocator *allocator, CommandTypeMask type_mask, CommandAllocatorFlag flags);
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
    APIResult create_swap_chain(SwapChain *swap_chain, SwapChainDesc *desc, Surface *surface);
    void delete_swap_chain(SwapChain *swap_chain);
    APIResult get_swap_chain_images(SwapChain *swap_chain, eastl::span<Image *> images);

    void wait_for_work();
    /// @returns - Success: APIResult::[Success, Timeout, NotReady, Suboptimal]
    /// @returns - Failure: APIResult::[OutOfHostMem, OutOfDeviceMem, DeviceLost, OutOfDate, SurfaceLost]
    APIResult acquire_next_image(SwapChain *swap_chain, u32 &image_id);
    /// @returns - Success: APIResult::[Success, Timeout, Suboptimal]
    /// @returns - Failure: APIResult::[OutOfHostMem, OutOfDeviceMem, DeviceLost, OutOfDate, SurfaceLost]
    APIResult present(SwapChain *swap_chain, CommandQueue *queue);

    /// PIPELINE ///
    PipelineLayout create_pipeline_layout(eastl::span<DescriptorSetLayout> layouts, eastl::span<PushConstantDesc> push_constants);
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
    DescriptorSetLayout create_descriptor_set_layout(eastl::span<DescriptorLayoutElement> elements, DescriptorSetLayoutFlag flags);
    void delete_descriptor_set_layout(DescriptorSetLayout layout);
    u64 get_descriptor_set_layout_size(DescriptorSetLayout layout);
    u64 get_descriptor_set_layout_binding_offset(DescriptorSetLayout layout, u32 binding_id);
    void get_descriptor_data(const DescriptorGetInfo &info, u64 data_size, void *data_out);

    /// RESOURCE ///
    eastl::tuple<u64, u64> get_buffer_memory_size(Buffer *buffer);
    eastl::tuple<u64, u64> get_image_memory_size(Image *image);

    /// @returns - Success: APIResult::Success
    /// @returns - Failure: APIResult::[OutOfHostMem, OutOfDeviceMem, InvalidExternalHandle]
    APIResult create_device_memory(DeviceMemory *device_memory, DeviceMemoryDesc *desc);
    void delete_device_memory(DeviceMemory *device_memory);
    void bind_memory(DeviceMemory *device_memory, Buffer *buffer, u64 memory_offset);
    void bind_memory(DeviceMemory *device_memory, Image *image, u64 memory_offset);

    // * Buffers * //
    /// @returns - Success: APIResult::Success
    /// @returns - Failure: APIResult::[OutOfHostMem, OutOfDeviceMem]
    APIResult create_buffer(Buffer *buffer, BufferDesc *desc);
    void delete_buffer(Buffer *handle);

    // * Images * //
    /// @returns - Success: APIResult::Success
    /// @returns - Failure: APIResult::[OutOfHostMem, OutOfDeviceMem]
    APIResult create_image(Image *image, ImageDesc *desc);
    void delete_image(Image *image);
    /// @returns - Success: APIResult::Success
    /// @returns - Failure: APIResult::[OutOfHostMem, OutOfDeviceMem]
    APIResult create_image_view(ImageView *image_view, ImageViewDesc *desc);
    void delete_image_view(ImageView *image_view);

    /// @returns - Success: APIResult::Success
    /// @returns - Failure: APIResult::[OutOfHostMem, OutOfDeviceMem]
    APIResult create_sampler(Sampler *sampler, SamplerDesc *desc);
    void delete_sampler(Sampler *sampler);

    /// UTILITY ///

    template<typename TType>
    void set_object_name(TType *obj, eastl::string_view name)
    {
#if _DEBUG
        VkDebugUtilsObjectNameInfoEXT object_name_info = {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
            .objectType = (VkObjectType)ToVKObjectType<TType>::type,
            .objectHandle = (u64)obj->m_handle,
            .pObjectName = name.data(),
        };
        vkSetDebugUtilsObjectNameEXT(m_handle, &object_name_info);
#endif
    }

    template<VkObjectType TObjectType, typename TType>
    void set_object_name_raw(TType object, eastl::string_view name)
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

    PhysicalDevice *m_physical_device = nullptr;
    TracyVkCtx m_tracy_ctx = nullptr;
};

}  // namespace lr::Graphics