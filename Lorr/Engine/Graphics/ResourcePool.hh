#pragma once

#include "Pipeline.hh"
#include "Resource.hh"
#include "Shader.hh"

namespace lr::graphics {
template<typename ResourceT, typename ResourceID>
struct allocation_result {
    allocation_result() = default;
    allocation_result(ResourceT *res, ResourceID res_id)
        : resource(res),
          id(res_id)
    {
    }

    ResourceT *resource = nullptr;
    ResourceID id = ResourceID::Invalid;

    explicit operator bool() { return id != ResourceID::Invalid; }
};

// Paged resource pool, a bit cleaner implementation to linked lists.
// is from Daxa
template<typename ResourceT, typename ResourceID, usize MAX_RESOURCE_COUNT = 1 << 20u>
struct PagedResourcePool {
    static_assert(std::is_default_constructible_v<ResourceT>, "ResourceT must have default constructor.");

    using index_type = std::underlying_type_t<ResourceID>;

    constexpr static index_type PAGE_BITS = 10u;
    constexpr static index_type PAGE_SIZE = 1 << PAGE_BITS;
    constexpr static index_type PAGE_MASK = PAGE_SIZE - 1u;
    constexpr static index_type PAGE_COUNT = MAX_RESOURCE_COUNT / PAGE_SIZE;
    using Page = std::array<ResourceT, PAGE_SIZE>;

    template<typename... Args>
    allocation_result<ResourceT, ResourceID> create(Args &&...args)
    {
        index_type index = static_cast<index_type>(~0);
        if (m_free_indexes.empty()) {
            index = m_latest_index++;
            if (index >= MAX_RESOURCE_COUNT) {
                return {};
            }
        }
        else {
            index = m_free_indexes.back();
            m_free_indexes.pop_back();
        }

        index_type page_id = index >> PAGE_BITS;
        index_type page_offset = index & PAGE_MASK;

        if (page_id >= PAGE_COUNT) {
            return {};
        }

        auto &page = m_pages[page_id];
        if (!page) {
            page = std::make_unique<Page>();
        }

        ResourceT *resource = &page->at(index);
        std::construct_at(reinterpret_cast<ResourceT *>(resource), std::forward<Args>(args)...);
        return { resource, static_cast<ResourceID>(index) };
    }

    void destroy(ResourceID id)
    {
        index_type index = get_handle_val(id);
        index_type page_id = index >> PAGE_BITS;
        index_type page_offset = index & PAGE_MASK;

        m_pages[page_id]->at(page_offset) = {};
        m_free_indexes.push_back(index);
    }

    ResourceT *get(ResourceID id)
    {
        index_type index = get_handle_val(id);
        index_type page_id = index >> PAGE_BITS;
        index_type page_offset = index & PAGE_MASK;

        return &m_pages[page_id]->at(page_offset);
    }

    void reset()
    {
        m_latest_index = 0;
        m_free_indexes.clear();
    }

    index_type max_resources() { return MAX_RESOURCE_COUNT; }

    index_type m_latest_index = 0;
    std::vector<index_type> m_free_indexes = {};
    std::array<std::unique_ptr<Page>, PAGE_COUNT> m_pages = {};
};

struct ResourcePools {
    PagedResourcePool<Buffer, BufferID> buffers = {};
    PagedResourcePool<Image, ImageID> images = {};
    PagedResourcePool<ImageView, ImageViewID> image_views = {};
    PagedResourcePool<Sampler, SamplerID> samplers = {};
    PagedResourcePool<Shader, ShaderID> shaders = {};
    PagedResourcePool<Pipeline, PipelineID> pipelines = {};

    constexpr static usize max_push_constant_size = 128;
    std::array<PipelineLayout, (max_push_constant_size / sizeof(u32)) + 1> pipeline_layouts = {};
};

/*
template<typename ResourceT, typename ResourceID>
struct PersistentResourcePool {
    PersistentResourcePool(u64 mem_size, u64 max_allocs)
    {
        u64 page_size = memory::page_size();
        u64 mem_size_aligned = memory::align_up(mem_size, page_size);
        m_data = memory::reserve(mem_size_aligned);
        if (!memory::commit(m_data, mem_size_aligned)) {
            m_data = nullptr;
            memory::release(m_data);
            return;
        }

        m_allocator = memory::TLSFAllocator(mem_size_aligned, max_allocs);
        uptr data_ptr = (uptr)m_data;
        data_ptr = memory::align_up(data_ptr, alignof(ResourceT));
        m_data = (void *)data_ptr;
    }

    ~PersistentResourcePool()
    {
        if (m_data) {
            memory::release(m_data);
        }
    }

    allocation_result<ResourceT &, ResourceID> allocate()
    {
        auto block_id = m_allocator.allocate(sizeof(ResourceT));
        if (block_id == memory::TLSFAllocator<>::MemoryBlock::INVALID) {
            return std::nullopt;
        }

        auto &block_data = m_allocator.get_block_data(block_id);
        u64 offset = memory::align_up(block_data.offset);

        auto resource = new ((u8 *)m_data + offset) ResourceT;
        return make_result(*resource, (ResourceID)block_id);
    }

    bool is_ok() { return m_data != nullptr; }

    memory::TLSFAllocator<> m_allocator;
    void *m_data = nullptr;
};
*/

}  // namespace lr::graphics
