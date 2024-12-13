#pragma once

#include "Engine/Graphics/Vulkan.hh"

#include "Engine/Graphics/Slang/Compiler.hh"
#include "Engine/Memory/PagedPool.hh"

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
namespace vk {
    auto get_vk_surface(VkInstance instance, void *handle) -> std::expected<VkSurfaceKHR, vk::Result>;
}

bool load_vulkan_instance(VkInstance instance, PFN_vkGetInstanceProcAddr get_instance_proc_addr);
bool load_vulkan_device(VkDevice device, PFN_vkGetDeviceProcAddr get_device_proc_addr);

enum class Descriptors : u32 {
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
    //OUR FORMAT                             COMP BLKW BLKH  VULKAN FORMAT
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
    BufferID id = BufferID::Invalid;

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
    ImageView default_view = {};
    u32 slice_count = 1;
    u32 mip_levels = 1;
    ImageID id = ImageID::Invalid;

    VmaAllocation allocation = {};
    VkImage handle = VK_NULL_HANDLE;
};

template<>
struct Handle<ImageView>::Impl {
    Device device = {};

    vk::Format format = vk::Format::Undefined;
    vk::ImageViewType type = vk::ImageViewType::View2D;
    vk::ImageSubresourceRange subresource_range = {};
    ImageViewID id = ImageViewID::Invalid;

    VkImageView handle = VK_NULL_HANDLE;
};

template<>
struct Handle<Sampler>::Impl {
    Device device = {};

    SamplerID id = SamplerID::Invalid;

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
    PipelineID id = PipelineID::Invalid;

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

template<typename ResourceT>
struct SubmittedResource {
    ResourceT resource;
    u64 sema_value;
};

template<>
struct Handle<CommandQueue>::Impl {
    Device device = {};

    vk::CommandType type = vk::CommandType::Count;
    u32 family_index = 0;  // Physical device queue family index
    Semaphore semaphore = {};

    std::deque<CommandList::Impl> command_list_pool = {};
    std::vector<u32> free_command_lists = {};
    std::vector<VkCommandBufferSubmitInfo> cmd_list_submits = {};

    plf::colony<SubmittedResource<BufferID>> garbage_buffers = {};
    plf::colony<SubmittedResource<ImageID>> garbage_images = {};
    plf::colony<SubmittedResource<ImageViewID>> garbage_image_views = {};
    plf::colony<SubmittedResource<SamplerID>> garbage_samplers = {};
    plf::colony<SubmittedResource<u32>> garbage_command_lists = {};

    VkQueue handle = VK_NULL_HANDLE;
};

template<>
struct Handle<Surface>::Impl {
    VkSurfaceKHR handle = VK_NULL_HANDLE;
};

template<>
struct Handle<SwapChain>::Impl {
    Device device = {};

    u32 acquired_index = 0;
    vk::Format format = vk::Format::Undefined;
    vk::Extent3D extent = {};

    ls::static_vector<Semaphore, Device::Limits::FrameCount> acquire_semas = {};
    ls::static_vector<Semaphore, Device::Limits::FrameCount> present_semas = {};

    Surface surface = {};
    vkb::Swapchain handle = {};
};

struct ActiveGPUAllocation {
    BufferID buffer_id = BufferID::Invalid;
    u32 size = 0;
    u32 offset = 0;
    u64 sema_counter = 0;
};

struct GPUAllocationNode {
    using NodeID = TransferManager::NodeID;

    ls::option<NodeID> prev_bin = ls::nullopt;
    ls::option<NodeID> next_bin = ls::nullopt;
    ls::option<NodeID> prev_neighbor = ls::nullopt;
    ls::option<NodeID> next_neighbor = ls::nullopt;
    u32 size = 0;
    u32 offset : 31 = 0;
    bool used : 1 = false;
};

template<>
struct Handle<TransferManager>::Impl {
    Device device = {};

    Semaphore semaphore = {};
    Buffer cpu_buffer = {};
    Buffer gpu_buffer = {};
    u32 size = 0;
    u32 max_allocs = 0;
    u32 free_storage = 0;

    u32 used_bins_top = {};
    u8 used_bins[TransferManager::NUM_TOP_BINS] = {};
    ls::option<TransferManager::NodeID> bin_indices[TransferManager::NUM_LEAF_BINS] = {};

    std::deque<SubmittedResource<u32>> tracked_nodes = {};
    std::vector<GPUAllocationNode> nodes = {};
    std::vector<TransferManager::NodeID> free_nodes = {};
    u32 free_offset = 0;
};

struct ResourcePools {
    PagedPool<Buffer::Impl, BufferID> buffers = {};
    PagedPool<Image::Impl, ImageID> images = {};
    PagedPool<ImageView::Impl, ImageViewID> image_views = {};
    PagedPool<Sampler::Impl, SamplerID, 256_sz> samplers = {};
    std::vector<ls::pair<u64, SamplerID>> cached_samplers = {};
    PagedPool<Pipeline::Impl, PipelineID> pipelines = {};

    constexpr static usize MAX_PUSH_CONSTANT_SIZE = 128;
    std::array<VkPipelineLayout, (MAX_PUSH_CONSTANT_SIZE / sizeof(u32)) + 1> pipeline_layouts = {};
};

template<>
struct Handle<Device>::Impl {
    constexpr static usize QUEUE_COUNT = static_cast<usize>(vk::CommandType::Count);

    std::array<CommandQueue, QUEUE_COUNT> queues = {};
    Semaphore frame_sema = {};
    usize frame_count = 0;  // global frame count, same across all swap chains

    VkDescriptorPool descriptor_pool = {};
    VkDescriptorSetLayout descriptor_set_layout = {};
    VkDescriptorSet descriptor_set = {};
    Buffer bda_array_buffer = {};
    TransferManager transfer_manager = {};

    SlangCompiler shader_compiler = {};
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
