#pragma once

#include "Engine/Graphics/Vulkan.hh"

#include "Engine/Graphics/ResourcePool.hh"
#include "Engine/Graphics/StagingBuffer.hh"

#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include <vulkan/vulkan.h>

#include <VkBootstrap.h>

#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#define VMA_VULKAN_VERSION 1003000
#include <vk_mem_alloc.h>

#define LR_VULKAN_INSTANCE_FUNC(name, ...) extern PFN_##name name;
#define LR_VULKAN_DEVICE_FUNC(name, ...) extern PFN_##name name;
#include "Engine/Graphics/VulkanFunctions.hh"
#undef LR_VULKAN_DEVICE_FUNC
#undef LR_VULKAN_INSTANCE_FUNC

namespace lr {
bool load_vulkan_instance(VkInstance instance, PFN_vkGetInstanceProcAddr get_instance_proc_addr);
bool load_vulkan_device(VkDevice device, PFN_vkGetDeviceProcAddr get_device_proc_addr);

enum Descriptors : u32 {
    Samplers = 0,
    Images,
    StorageImages,
    StorageBuffers,
    BDA,
};

struct FormatInfo {
    vk::Format format = vk::Format::Undefined;
    u32 component_size = 0;
    u32 block_width = 1;
    u32 block_height = 1;
    VkFormat vk_format = VK_FORMAT_UNDEFINED;
};

constexpr static FormatInfo FORMAT_INFOS[] = {
    // clang-format off
    { vk::Format::Undefined,                    0,   1,   1, VK_FORMAT_UNDEFINED },
    { vk::Format::R8_UNORM,                     1,   1,   1, VK_FORMAT_R8_UNORM },
    { vk::Format::R8G8B8A8_UNORM,               4,   1,   1, VK_FORMAT_R8G8B8A8_UNORM },
    { vk::Format::R8G8B8A8_SINT,                4,   1,   1, VK_FORMAT_R8G8B8A8_SINT },
    { vk::Format::R8G8B8A8_UINT,                4,   1,   1, VK_FORMAT_R8G8B8A8_UINT },
    { vk::Format::R8G8B8A8_SRGB,                4,   1,   1, VK_FORMAT_R8G8B8A8_SRGB },
    { vk::Format::B8G8R8A8_UNORM,               4,   1,   1, VK_FORMAT_B8G8R8A8_UNORM },
    { vk::Format::B8G8R8A8_SINT,                4,   1,   1, VK_FORMAT_B8G8R8A8_SINT },
    { vk::Format::B8G8R8A8_UINT,                4,   1,   1, VK_FORMAT_B8G8R8A8_UINT },
    { vk::Format::B8G8R8A8_SRGB,                4,   1,   1, VK_FORMAT_B8G8R8A8_SRGB },
    { vk::Format::R32_SFLOAT,                   4,   1,   1, VK_FORMAT_R32_SFLOAT },
    { vk::Format::R32_SINT,                     4,   1,   1, VK_FORMAT_R32_SINT },
    { vk::Format::R32_UINT,                     4,   1,   1, VK_FORMAT_R32_UINT },
    { vk::Format::D32_SFLOAT,                   4,   1,   1, VK_FORMAT_D32_SFLOAT },
    { vk::Format::D24_UNORM_S8_UINT,            4,   1,   1, VK_FORMAT_D24_UNORM_S8_UINT },
    { vk::Format::R16G16B16A16_SFLOAT,          8,   1,   1, VK_FORMAT_R16G16B16A16_SFLOAT },
    { vk::Format::R16G16B16A16_SINT,            8,   1,   1, VK_FORMAT_R16G16B16A16_SINT },
    { vk::Format::R16G16B16A16_UINT,            8,   1,   1, VK_FORMAT_R16G16B16A16_UINT },
    { vk::Format::R32G32_SFLOAT,                8,   1,   1, VK_FORMAT_R32G32_SFLOAT },
    { vk::Format::R32G32_SINT,                  8,   1,   1, VK_FORMAT_R32G32_SINT },
    { vk::Format::R32G32_UINT,                  8,   1,   1, VK_FORMAT_R32G32_UINT },
    { vk::Format::R32G32B32_SFLOAT,            12,   1,   1, VK_FORMAT_R32G32B32_SFLOAT },
    { vk::Format::R32G32B32_SINT,              12,   1,   1, VK_FORMAT_R32G32B32_SINT },
    { vk::Format::R32G32B32_UINT,              12,   1,   1, VK_FORMAT_R32G32B32_UINT },
    { vk::Format::R32G32B32A32_SFLOAT,         16,   1,   1, VK_FORMAT_R32G32B32A32_SFLOAT },
    { vk::Format::R32G32B32A32_SINT,           16,   1,   1, VK_FORMAT_R32G32B32A32_SINT },
    { vk::Format::R32G32B32A32_UINT,           16,   1,   1, VK_FORMAT_R32G32B32A32_UINT },
    // clang-format on
};

constexpr static const auto &get_format_info(vk::Format f) {
    return FORMAT_INFOS[std::to_underlying(f)];
}

template<>
struct Handle<Buffer>::Impl {
    Device device = {};

    u64 data_size = 0;
    void *host_data = nullptr;
    u64 device_address = 0;

    VmaAllocation allocation = {};
    VkBuffer handle = VK_NULL_HANDLE;
};

template<>
struct Handle<Image>::Impl {
    Device device = {};

    vk::ImageUsage usage_flags = {};
    vk::Format format = vk::Format::Undefined;
    vk::ImageAspectFlag aspect_flags = vk::ImageAspectFlag::Color;
    vk::Extent3D extent = {};
    u32 slice_count = 1;
    u32 mip_levels = 1;

    VmaAllocation allocation = {};
    VkImage handle = VK_NULL_HANDLE;
};

template<>
struct Handle<ImageView>::Impl {
    Device device = {};

    vk::Format format = vk::Format::Undefined;
    vk::ImageViewType type = vk::ImageViewType::View2D;
    vk::ImageSubresourceRange subresource_range = {};

    VkImageView handle = VK_NULL_HANDLE;
};

template<>
struct Handle<Sampler>::Impl {
    Device device = {};

    VkSampler handle = VK_NULL_HANDLE;
};

template<>
struct Handle<Shader>::Impl {
    Device device = {};
    vk::ShaderStageFlag stage = vk::ShaderStageFlag::Count;

    VkShaderModule handle = VK_NULL_HANDLE;
};

template<>
struct Handle<Pipeline>::Impl {
    Device device = {};

    vk::PipelineBindPoint bind_point = vk::PipelineBindPoint::Graphics;
    PipelineLayoutID layout_id = PipelineLayoutID::None;

    VkPipeline handle = VK_NULL_HANDLE;
};

template<>
struct Handle<Semaphore>::Impl {
    Device device = {};

    VkSemaphore handle = VK_NULL_HANDLE;
    u64 value = 0;
};

template<>
struct Handle<QueryPool>::Impl {
    Device device = {};

    VkQueryPool handle = VK_NULL_HANDLE;
};

template<>
struct Handle<CommandList>::Impl {
    Device device = {};

    u32 id = ~0_u32;
    vk::CommandType type = vk::CommandType::Count;
    ls::option<Pipeline> bound_pipeline = ls::nullopt;
    ls::option<VkPipelineLayout> bound_pipeline_layout = ls::nullopt;
    VkPipelineBindPoint bind_point = VK_PIPELINE_BIND_POINT_MAX_ENUM;

    VkCommandPool allocator = VK_NULL_HANDLE;
    VkCommandBuffer handle = VK_NULL_HANDLE;
};

template<>
struct Handle<CommandQueue>::Impl {
    Device device = {};

    vk::CommandType type = vk::CommandType::Count;
    u32 family_index = 0;  // Physical device queue family index
    Semaphore semaphore = {};

    PagedResourcePool<CommandList::Impl, u32, { .MAX_RESOURCE_COUNT = 2048u, .PAGE_BITS = 32u }> command_lists = {};
    std::vector<VkCommandBufferSubmitInfo> frame_cmd_list_infos = {};

    plf::colony<std::pair<BufferID, u64>> garbage_buffers = {};
    plf::colony<std::pair<ImageID, u64>> garbage_images = {};
    plf::colony<std::pair<ImageViewID, u64>> garbage_image_views = {};
    plf::colony<std::pair<SamplerID, u64>> garbage_samplers = {};
    plf::colony<std::pair<u32, u64>> garbage_command_lists = {};

    VkQueue handle = VK_NULL_HANDLE;
};

template<>
struct Handle<Surface>::Impl {
    VkSurfaceKHR handle = VK_NULL_HANDLE;
};

template<>
struct Handle<SwapChain>::Impl {
    vk::Format format = vk::Format::Undefined;
    vk::Extent3D extent = {};

    ls::static_vector<Semaphore, Device::Limits::FrameCount> acquire_semas = {};
    ls::static_vector<Semaphore, Device::Limits::FrameCount> present_semas = {};

    Surface surface = {};
    vkb::Swapchain handle = {};
};

struct ResourcePools {
    PagedResourcePool<Buffer::Impl, BufferID> buffers = {};
    PagedResourcePool<Image::Impl, ImageID> images = {};
    PagedResourcePool<ImageView::Impl, ImageViewID> image_views = {};
    PagedResourcePool<Sampler::Impl, SamplerID, { .MAX_RESOURCE_COUNT = 512u, .PAGE_BITS = 32u }> samplers = {};
    PagedResourcePool<Pipeline::Impl, PipelineID> pipelines = {};

    constexpr static usize max_push_constant_size = 128;
    std::array<VkPipelineLayout, (max_push_constant_size / sizeof(u32)) + 1> pipeline_layouts = {};
};

template<>
struct Handle<Device>::Impl {
    constexpr static usize QUEUE_COUNT = static_cast<usize>(vk::CommandType::Count);
    constexpr static u64 STAGING_UPLOAD_SIZE = ls::mib_to_bytes(128);

    std::array<CommandQueue, QUEUE_COUNT> queues = {};
    ls::static_vector<StagingBuffer, Device::Limits::FrameCount> staging_buffers;
    Semaphore frame_sema = {};
    usize frame_count = 0;  // global frame count, same across all swap chains

    VkDescriptorPool descriptor_pool = {};
    VkDescriptorSetLayout descriptor_set_layout = {};
    VkDescriptorSet descriptor_set = {};

    BufferID bda_array_buffer = BufferID::Invalid;
    u64 *bda_array_host_addr = nullptr;

    DeviceFeature supported_features = DeviceFeature::None;
    ResourcePools resources = {};
    VmaAllocator allocator = {};

    vkb::Instance instance = {};
    vkb::PhysicalDevice physical_device = {};
    vkb::Device handle = {};
};

template<typename T>
static void set_object_name(Device_H device, T v, VkObjectType object_type, std::string_view name) {
    VkDebugUtilsObjectNameInfoEXT object_name_info = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .pNext = nullptr,
        .objectType = object_type,
        .objectHandle = ls::bit_cast<u64>(v),
        .pObjectName = name.data(),
    };
    vkSetDebugUtilsObjectNameEXT(device->handle, &object_name_info);
}

}  // namespace lr
