#pragma once

namespace lr {
struct PagedResourcePoolInfo {
    usize MAX_RESOURCE_COUNT = 1u << 19u;
    usize PAGE_BITS = 9u;
};

template<typename ResourceT, typename ResourceID, PagedResourcePoolInfo INFO = {}>
struct PagedResourcePool {
    constexpr static usize MAX_RESOURCE_COUNT = INFO.MAX_RESOURCE_COUNT;
    constexpr static usize PAGE_BITS = INFO.PAGE_BITS;
    constexpr static usize PAGE_SIZE = 1_sz << PAGE_BITS;
    constexpr static usize PAGE_MASK = PAGE_SIZE - 1_sz;
    constexpr static usize PAGE_COUNT = MAX_RESOURCE_COUNT / PAGE_SIZE;
    using Page = std::array<ResourceT, PAGE_SIZE>;

    struct Result {
        ResourceT *impl = nullptr;
        ResourceID id = {};
    };

    u32 latest_index = 0;
    std::vector<u32> free_indexes = {};
    std::array<std::unique_ptr<Page>, PAGE_COUNT> pages = {};

    ls::option<Result> create(this auto &self) {
        ZoneScoped;

        u32 index = 0;
        if (self.free_indexes.empty()) {
            index = self.latest_index++;
            if (index >= MAX_RESOURCE_COUNT) {
                return ls::nullopt;
            }
        } else {
            index = self.free_indexes.back();
            self.free_indexes.pop_back();
        }

        auto page_id = static_cast<usize>(index) >> PAGE_BITS;
        if (page_id >= PAGE_COUNT) {
            return ls::nullopt;
        }

        auto &page = self.pages[page_id];
        if (!page) {
            page = std::make_unique<Page>();
        }

        return Result(&page->at(index), static_cast<ResourceID>(index));
    }

    void destroy(this auto &self, ResourceID id) {
        ZoneScoped;

        u32 index = static_cast<u32>(id);
        auto page_id = static_cast<usize>(index) >> PAGE_BITS;
        auto page_offset = static_cast<usize>(index) & PAGE_MASK;

        self.pages[page_id]->at(page_offset) = {};
        self.free_indexes.push_back(index);
    }

    ResourceT &get(this auto &self, ResourceID id) {
        ZoneScoped;

        auto index = static_cast<usize>(id);
        auto page_id = index >> PAGE_BITS;
        auto page_offset = index & PAGE_MASK;

        return self.pages[page_id]->at(page_offset);
    }

    void reset(this auto &self) {
        ZoneScoped;

        self.latest_index = 0;
        self.free_indexes.clear();
    }

    u32 max_resources() { return MAX_RESOURCE_COUNT; }
};

}  // namespace lr
