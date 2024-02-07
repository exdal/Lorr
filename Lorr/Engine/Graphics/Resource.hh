#pragma once

#include "APIObject.hh"
#include "Common.hh"

namespace lr::Graphics {
/////////////////////////////////
// BUFFERS

LR_HANDLE(BufferID, u64);
struct BufferDesc {
    BufferUsage m_usage_flags = BufferUsage::Vertex;
    u64 m_data_size = 0;
};

struct Buffer {
    void init(VkBuffer buffer, u64 size, u64 device_address)
    {
        m_handle = buffer;
        m_size = size;
        m_device_address = device_address;
    }

    u64 m_alignment = 0;
    u64 m_size = 0;
    u64 m_device_address = 0;
    union {
        u64 m_allocator_data = ~0;
        struct {
            u32 m_pool_id;
            u32 m_memory_id;
        };
    };
    VkBuffer m_handle = nullptr;

    operator VkBuffer &() { return m_handle; }
    explicit operator bool() { return m_handle != nullptr; }
};
LR_ASSIGN_OBJECT_TYPE(Buffer, VK_OBJECT_TYPE_BUFFER);

/////////////////////////////////
// IMAGES

LR_HANDLE(ImageID, u64);
struct ImageDesc {
    ImageUsage m_usage_flags = ImageUsage::Sampled;
    Format m_format = Format::Unknown;
    ImageType m_type = ImageType::View2D;

    u32 m_width = 0;
    u32 m_height = 0;
    u32 m_slice_count = 1;
    u32 m_mip_levels = 1;
    eastl::span<u32> m_queue_indices = {};

    u64 m_data_size = 0;
};

struct Image {
    void init(VkImage image, Format format, u32 width, u32 height, u32 slice_count, u32 mip_levels)
    {
        m_handle = image;
        m_format = format;
        m_width = width;
        m_height = height;
        m_slice_count = slice_count;
        m_mip_levels = mip_levels;
    }

    Format m_format = Format::Unknown;
    u32 m_width = 0;
    u32 m_height = 0;
    u32 m_slice_count = 1;
    u32 m_mip_levels = 1;

    bool m_swap_chain_image = false;
    u64 m_allocator_data = ~0;

    VkImage m_handle = VK_NULL_HANDLE;
};
LR_ASSIGN_OBJECT_TYPE(Image, VK_OBJECT_TYPE_IMAGE);

// Image views can have different formats depending on the creation flags
// Currently, we do not support it so view's format is automatically parent
// image format. See:
// https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkImageViewCreateInfo.html

LR_HANDLE(ImageViewID, u64);
struct ImageSubresourceInfo {
    ImageAspect m_aspect_mask = ImageAspect::Color;
    u32 m_base_mip : 8 = 0;
    u32 m_mip_count : 8 = 1;
    u32 m_base_slice : 8 = 0;
    u32 m_slice_count : 8 = 1;
};

struct ImageViewDesc {
    Image *m_image = nullptr;
    ImageViewType m_type = ImageViewType::View2D;
    // Component mapping
    ImageComponentSwizzle m_swizzle_r = ImageComponentSwizzle::Identity;
    ImageComponentSwizzle m_swizzle_g = ImageComponentSwizzle::Identity;
    ImageComponentSwizzle m_swizzle_b = ImageComponentSwizzle::Identity;
    ImageComponentSwizzle m_swizzle_a = ImageComponentSwizzle::Identity;
    ImageSubresourceInfo m_subresource_info = {};
};

struct ImageSubresourceRange : VkImageSubresourceRange {
    ImageSubresourceRange(ImageSubresourceInfo info);
};

struct ImageView {
    void init(VkImageView image_view_handle, Format format, ImageViewType type, const ImageSubresourceInfo &subresource_info)
    {
        m_handle = image_view_handle;
        m_format = format;
        m_type = type;
        m_subresource_info = subresource_info;
    }

    Format m_format = Format::Unknown;
    ImageViewType m_type = ImageViewType::View2D;
    ImageSubresourceInfo m_subresource_info = {};

    VkImageView m_handle = VK_NULL_HANDLE;
};
LR_ASSIGN_OBJECT_TYPE(ImageView, VK_OBJECT_TYPE_IMAGE_VIEW);

/////////////////////////////////
// SAMPLERS

LR_HANDLE(SamplerID, u64);
struct SamplerDesc {
    Filtering m_min_filter : 1 = {};
    Filtering m_mag_filter : 1 = {};
    Filtering m_mip_filter : 1 = {};
    TextureAddressMode m_address_u : 2 = {};
    TextureAddressMode m_address_v : 2 = {};
    TextureAddressMode m_address_w : 2 = {};
    u32 m_use_anisotropy : 1 = {};
    CompareOp m_compare_op : 3 = {};
    float m_max_anisotropy = 0;
    float m_mip_lod_bias = 0;
    float m_min_lod = 0;
    float m_max_lod = 0;
};

struct Sampler {
    void init(VkSampler sampler_handle) { m_handle = sampler_handle; }

    VkSampler m_handle = VK_NULL_HANDLE;
};
LR_ASSIGN_OBJECT_TYPE(Sampler, VK_OBJECT_TYPE_SAMPLER);

template<typename ResourceT, typename ResourceID>
struct ResourcePool {
    static constexpr u64 FL_SIZE = 64u;
    static constexpr u64 SL_SIZE = 64u;
    static constexpr u64 SL_MASK = ~SL_SIZE;
    static constexpr u64 SL_SHIFT = SL_SIZE - 1;
    static constexpr u64 RESOURCE_COUNT = FL_SIZE * SL_SIZE;

    ResourcePool() { m_resources.resize(RESOURCE_COUNT); }
    ~ResourcePool() = default;

    eastl::tuple<ResourceID, ResourceT *> add_resource()
    {
        ZoneScoped;

        if (m_first_list == eastl::numeric_limits<decltype(m_first_list)>::max()) {
            LOG_ERROR("Resource pool overflow");
            return { ResourceID::Invalid, nullptr };
        }

        u64 first_index = Memory::find_lsb(~m_first_list);
        u64 second_list = m_second_list[first_index];
        u64 second_index = Memory::find_lsb(~second_list);

        second_list |= (u64)1 << second_index;
        if (_popcnt64(second_list) == SL_SIZE)
            m_first_list |= (u64)1 << first_index;

        m_second_list[first_index] = second_list;
        auto resource_id = set_handle_val<ResourceID>(get_index(first_index, second_index));
        return { resource_id, &get_resource(resource_id) };
    }

    void remove_resource(ResourceID id)
    {
        ZoneScoped;

        auto val = get_handle_val(id);
        u64 first_index = val >> SL_SHIFT;
        u64 second_index = val & SL_MASK;
        u64 second_list = m_second_list[first_index];

        second_list &= ~((u64)1 << second_index);
        if (_popcnt64(second_list) != SL_SIZE)
            m_first_list &= ~((u64)1 << first_index);

        m_second_list[first_index] = second_list;
    }

    bool validate_id(ResourceID id)
    {
        ZoneScoped;

        if (!id)
            return false;

        auto val = get_handle_val(id);
        u64 first_index = val >> SL_SHIFT;
        u64 second_index = val & SL_MASK;
        u64 second_list = m_second_list[first_index];

        return (second_list >> second_index) & 0x1;
    }

    static u64 get_index(u64 fi, u64 si) { return fi == 0 ? si : fi * SL_SIZE + si; }
    ResourceT &get_resource(ResourceID id) { return m_resources[get_handle_val(id)]; }

    u64 m_first_list = 0;  // This bitmap indicates that SL[bit(i)] is full or not
    u64 m_second_list[FL_SIZE] = {};
    eastl::vector<ResourceT> m_resources = {};
};

struct ResourcePools {
    // vectors move memories on de/allocation, so using raw pointer is not safe
    // instead we have ResourcePool that is fixed size and does not do allocations
    ResourcePool<Buffer, BufferID> m_buffers = {};
    ResourcePool<Image, ImageID> m_images = {};
    ResourcePool<ImageView, ImageViewID> m_image_views = {};
    ResourcePool<Sampler, SamplerID> m_samplers = {};
};

}  // namespace lr::Graphics
