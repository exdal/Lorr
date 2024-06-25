#pragma once

#include "Pipeline.hh"
#include "Resource.hh"
#include "Shader.hh"

namespace lr {
template<typename ResourceT, typename ResourceID>
struct allocation_result {
    allocation_result(ResourceT &res, ResourceID res_id)
        : resource(res),
          id(res_id)
    {
    }

    ResourceT &resource;
    ResourceID id;
};

// Paged resource pool, a bit cleaner implementation to linked lists.
// is from Daxa
template<typename ResourceT, typename ResourceID>
struct PagedResourcePool {
    static_assert(std::is_default_constructible_v<ResourceT>, "ResourceT must have default constructor.");

    using index_type = std::underlying_type_t<ResourceID>;

    constexpr static index_type MAX_RESOURCE_COUNT = 1 << 19u;
    constexpr static index_type PAGE_BITS = 9u;
    constexpr static index_type PAGE_SIZE = 1 << PAGE_BITS;
    constexpr static index_type PAGE_MASK = PAGE_SIZE - 1u;
    constexpr static index_type PAGE_COUNT = MAX_RESOURCE_COUNT / PAGE_SIZE;
    using Page = std::array<ResourceT, PAGE_SIZE>;

    index_type latest_index = 0;
    std::vector<index_type> free_indexes = {};
    std::array<std::unique_ptr<Page>, PAGE_COUNT> pages = {};

    template<typename... Args>
    std::optional<allocation_result<ResourceT, ResourceID>> create(this auto &self, Args &&...args)
    {
        ZoneScoped;

        index_type index = static_cast<index_type>(~0);
        if (self.free_indexes.empty()) {
            index = self.latest_index++;
            if (index >= MAX_RESOURCE_COUNT) {
                return std::nullopt;
            }
        }
        else {
            index = self.free_indexes.back();
            self.free_indexes.pop_back();
        }

        index_type page_id = index >> PAGE_BITS;
        index_type page_offset = index & PAGE_MASK;

        if (page_id >= PAGE_COUNT) {
            return std::nullopt;
        }

        auto &page = self.pages[page_id];
        if (!page) {
            page = std::make_unique<Page>();
        }

        ResourceT &resource = page->at(page_offset);
        std::construct_at(reinterpret_cast<ResourceT *>(&resource), std::forward<Args>(args)...);
        return allocation_result(resource, static_cast<ResourceID>(index));
    }

    void destroy(this auto &self, ResourceID id)
    {
        ZoneScoped;

        index_type index = static_cast<index_type>(id);
        index_type page_id = index >> PAGE_BITS;
        index_type page_offset = index & PAGE_MASK;

        self.pages[page_id]->at(page_offset) = {};
        self.free_indexes.push_back(index);
    }

    ResourceT &get(this auto &self, ResourceID id)
    {
        ZoneScoped;

        index_type index = static_cast<index_type>(id);
        index_type page_id = index >> PAGE_BITS;
        index_type page_offset = index & PAGE_MASK;

        return self.pages[page_id]->at(page_offset);
    }

    void reset(this auto &self)
    {
        ZoneScoped;

        self.latest_index = 0;
        self.free_indexes.clear();
    }

    index_type max_resources() { return MAX_RESOURCE_COUNT; }
};

struct ResourcePools {
    PagedResourcePool<Buffer, BufferID> buffers = {};
    PagedResourcePool<Image, ImageID> images = {};
    PagedResourcePool<ImageView, ImageViewID> image_views = {};
    PagedResourcePool<Sampler, SamplerID> samplers = {};
    PagedResourcePool<Shader, ShaderID> shaders = {};
    PagedResourcePool<Pipeline, PipelineID> pipelines = {};

    ankerl::unordered_dense::map<SamplerHash, SamplerID> cached_samplers = {};
    ankerl::unordered_dense::map<PipelineHash, PipelineID> cached_pipelines = {};

    constexpr static usize max_push_constant_size = 128;
    std::array<PipelineLayout, (max_push_constant_size / sizeof(u32)) + 1> pipeline_layouts = {};
};

}  // namespace lr
