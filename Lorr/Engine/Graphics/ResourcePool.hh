#pragma once

namespace lr {
template<typename T>
concept EnumType = std::is_enum_v<T>;

template<typename T>
struct EnumAccessor {
    using type = T;
};

template<EnumType T>
struct EnumAccessor<T> {
    using type = std::underlying_type_t<T>;
};

struct PagedResourcePoolInfo {
    u32 MAX_RESOURCE_COUNT = 1 << 19u;
    u32 PAGE_BITS = 1 << 9u;
};

template<typename ResourceT, typename ResourceID, PagedResourcePoolInfo INFO = {}>
struct PagedResourcePool {
    using index_type = EnumAccessor<ResourceID>::type;

    constexpr static index_type PAGE_SIZE = INFO.PAGE_BITS;
    constexpr static index_type PAGE_MASK = PAGE_SIZE - 1u;
    constexpr static index_type PAGE_COUNT = INFO.MAX_RESOURCE_COUNT / PAGE_SIZE;
    using Page = std::array<ResourceT, PAGE_SIZE>;

    struct Result {
        ResourceT *impl = nullptr;
        ResourceID id = {};
        bool is_fresh = false;
    };

    index_type latest_index = 0;
    std::vector<index_type> free_indexes = {};
    std::array<std::unique_ptr<Page>, PAGE_COUNT> pages = {};

    ls::option<Result> create(this auto &self) {
        ZoneScoped;

        index_type index = static_cast<index_type>(~0);
        bool is_fresh = false;
        if (self.free_indexes.empty()) {
            index = self.latest_index++;
            is_fresh = true;
            if (index >= INFO.MAX_RESOURCE_COUNT) {
                return ls::nullopt;
            }
        } else {
            index = self.free_indexes.back();
            is_fresh = true;
            self.free_indexes.pop_back();
        }

        index_type page_id = index >> INFO.PAGE_BITS;
        index_type page_offset = index & PAGE_MASK;

        if (page_id >= PAGE_COUNT) {
            return ls::nullopt;
        }

        auto &page = self.pages[page_id];
        if (!page) {
            page = std::make_unique<Page>();
        }

        return { static_cast<ResourceID>(index), page.at(index), is_fresh };
    }

    void destroy(this auto &self, ResourceID id) {
        ZoneScoped;

        index_type index = static_cast<index_type>(id);
        index_type page_id = index >> INFO.PAGE_BITS;
        index_type page_offset = index & PAGE_MASK;

        self.pages[page_id]->at(page_offset) = nullptr;
        self.free_indexes.push_back(index);
    }

    void zombify(this auto &self, ResourceID id) {
        ZoneScoped;

        auto index = static_cast<index_type>(id);

        self.free_indexes.push_back(index);
    }

    ResourceT &get(this auto &self, ResourceID id) {
        ZoneScoped;

        index_type index = static_cast<index_type>(id);
        index_type page_id = index >> INFO.PAGE_BITS;
        index_type page_offset = index & PAGE_MASK;

        return self.pages[page_id]->at(page_offset);
    }

    void reset(this auto &self) {
        ZoneScoped;

        self.latest_index = 0;
        self.free_indexes.clear();
    }

    index_type max_resources() { return INFO.MAX_RESOURCE_COUNT; }
};

}  // namespace lr
