// Created on Monday July 18th 2022 by exdal
// Last modified on Monday August 28th 2023 by exdal

#pragma once

#include <EASTL/fixed_vector.h>
#include <EASTL/tuple.h>

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

struct Device
{
    Device(PhysicalDevice *physical_device, VkDevice handle);

    /// COMMAND ///
    CommandQueue *create_command_queue(CommandType type, u32 queue_index);
    CommandAllocator *create_command_allocator(u32 queue_idx, CommandAllocatorFlag flags);
    CommandList *create_command_list(CommandAllocator *command_allocator);

    void begin_command_list(CommandList *list);
    void end_command_list(CommandList *list);
    void reset_command_allocator(CommandAllocator *allocator);
    void submit(CommandQueue *queue, SubmitDesc *desc);

    Fence create_fence(bool signaled);
    void delete_fence(Fence fence);
    bool is_fence_signaled(Fence fence);
    void reset_fence(Fence fence);

    Semaphore *create_binary_semaphore();
    Semaphore *create_timeline_semaphore(u64 initial_value);
    void delete_semaphore(Semaphore *semaphore);
    void wait_for_semaphore(Semaphore *semaphore, u64 desired_value, u64 timeout = UINT64_MAX);

    /// PIPELINE ///
    VkPipelineCache create_pipeline_cache(u32 initial_data_size = 0, void *initial_data = nullptr);
    PipelineLayout create_pipeline_layout(eastl::span<DescriptorSetLayout> layouts, eastl::span<PushConstantDesc> push_constants);

    Pipeline *create_graphics_pipeline(GraphicsPipelineBuildInfo *build_info);
    Pipeline *create_compute_pipeline(ComputePipelineBuildInfo *build_info);

    /// SWAPCHAIN ///
    SwapChain *create_swap_chain(Surface *surface, SwapChainDesc *desc);
    void delete_swap_chain(SwapChain *swap_chain);
    ls::ref_array<Image *> get_swap_chain_images(SwapChain *swap_chain);

    void wait_for_work();
    APIResult acquire_image(SwapChain *swap_chain, Semaphore *semaphore, u32 &image_idx);
    void Present(SwapChain *swap_chain, u32 image_idx, Semaphore *semaphore, CommandQueue *queue);

    /// RESOURCE ///
    u64 get_buffer_memory_size(Buffer *buffer, u64 *alignment_out = nullptr);
    u64 get_image_memory_size(Image *image, u64 *alignment_out = nullptr);

    DeviceMemory *create_device_memory(DeviceMemoryDesc *desc, PhysicalDevice *physical_device);
    void delete_device_memory(DeviceMemory *device_memory);
    void allocate_buffer_memory(DeviceMemory *device_memory, Buffer *buffer, u64 memory_size);
    void allocate_image_memory(DeviceMemory *device_memory, Image *image, u64 memory_size);

    // * Shaders * //
    Shader *create_shader(ShaderStage stage, u32 *data, u64 data_size);
    void delete_shader(Shader *shader);

    // * Descriptor * //

    DescriptorSetLayout create_descriptor_set_layout(
        eastl::span<DescriptorLayoutElement> elements, DescriptorSetLayoutFlag flags = DescriptorSetLayoutFlag::DescriptorBuffer);
    void delete_descriptor_set_layout(DescriptorSetLayout layout);
    u64 get_descriptor_set_layout_size(DescriptorSetLayout layout);
    u64 get_descriptor_set_layout_binding_offset(DescriptorSetLayout layout, u32 binding_id);
    void get_descriptor_data(const DescriptorGetInfo &info, u64 data_size, void *data_out);

    // * Buffers * //
    Buffer *create_buffer(BufferDesc *desc, DeviceMemory *device_memory);
    void delete_buffer(Buffer *handle, DeviceMemory *device_memory);

    u8 *get_memory_data(DeviceMemory *device_memory);
    u8 *get_buffer_memory_data(DeviceMemory *device_memory, Buffer *buffer);

    // * Images * //
    Image *create_image(ImageDesc *desc, DeviceMemory *device_memory);
    void delete_image(Image *image, DeviceMemory *device_memory);
    ImageView *create_image_view(ImageViewDesc *desc);
    void delete_image_view(ImageView *image_view);

    Sampler *create_sampler(SamplerDesc *desc);
    void delete_sampler(VkSampler sampler);

    /// UTILITY ///
    template<typename TType>
    void SetObjectName(TType *obj, eastl::string_view name)
    {
#if _DEBUG
        VkDebugUtilsObjectNameInfoEXT object_name_info = {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
            .objectType = static_cast<VkObjectType>(ToVKObjectType<TType>::type),
            .objectHandle = (u64)obj->m_handle,
            .pObjectName = name.data(),
        };
        vkSetDebugUtilsObjectNameEXT(m_handle, &object_name_info);
#endif
    }

    template<VkObjectType TObjectType, typename TType>
    void SetObjectNameRaw(TType object, eastl::string_view name)
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

    VkDevice m_handle = nullptr;
    PhysicalDevice *m_physical_device = nullptr;
    TracyVkCtx m_tracy_ctx = nullptr;

    /// Pools/Caches
    VkPipelineCache m_pipeline_cache = nullptr;
};

}  // namespace lr::Graphics