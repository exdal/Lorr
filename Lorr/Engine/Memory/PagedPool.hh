#pragma once

#include "Engine/Memory/Bit.hh"

namespace lr {
template<typename T, typename ID, usize MAX_RESOURCE_COUNT = 1u << 19u>
struct PagedPool {
    constexpr static usize PAGE_BITS = 6_sz;
    constexpr static usize PAGE_SIZE = 1_sz << PAGE_BITS;
    constexpr static usize PAGE_MASK = PAGE_SIZE - 1_sz;
    constexpr static usize PAGE_COUNT = MAX_RESOURCE_COUNT / PAGE_SIZE;
    struct Page {
        std::array<T, PAGE_SIZE> elements;
        // availability mask, each bit represents state of `T` in `arr`.
        // 1 == alive, 0 = dead element.
        u64 mask = 0;
    };

    struct Result {
        T *self = nullptr;
        ID id = {};
    };

    ls::static_vector<std::unique_ptr<Page>, PAGE_COUNT> pages = {};

    auto create() -> ls::option<Result> {
        ZoneScoped;

        // Iterate over all allocated pages
        u32 page_index = 0;
        ls::option<u32> page_offset = ls::nullopt;
        for (u32 cur_page_index = 0; cur_page_index < this->pages.size(); cur_page_index++) {
            auto &cur_page = this->pages[cur_page_index];
            if (cur_page->mask == std::numeric_limits<u64>::max()) {
                page_index = cur_page_index + 1;
                continue;
            }

            page_index = cur_page_index;
            page_offset = memory::tzcnt_64(~cur_page->mask);
            break;
        }

        if (page_index >= PAGE_COUNT) {
            return ls::nullopt;
        }

        u32 resource_index = (page_index << PAGE_BITS);
        // All pages are full
        if (LS_UNLIKELY(!page_offset.has_value() && resource_index >= MAX_RESOURCE_COUNT)) {
            return ls::nullopt;
        } else {
            resource_index |= page_offset.value_or(0);
        }

        if (this->pages.size() <= page_index) {
            this->pages.resize(page_index + 1);
        }

        auto &page = this->pages[page_index];
        if (!page) {
            page = std::make_unique<Page>();
        }

        page->mask |= (1_u64 << page_offset.value_or(0));
        auto *resource = &page->elements[page_offset.value_or(0)];
        std::construct_at(resource);

        return Result(resource, static_cast<ID>(resource_index));
    }

    auto destroy(ID id) -> void {
        ZoneScoped;

        auto index = static_cast<u32>(id);
        auto page_index = static_cast<usize>(index) >> PAGE_BITS;
        auto page_offset = static_cast<usize>(index) & PAGE_MASK;
        LS_EXPECT(this->pages.size() > page_index);

        auto &page = this->pages[page_index];
        std::destroy_at(&page->elements[page_offset]);
        page->mask &= ~(1_u64 << page_offset);
    }

    auto get(ID id) -> T & {
        ZoneScoped;

        auto index = static_cast<u32>(id);
        auto page_index = static_cast<usize>(index) >> PAGE_BITS;
        auto page_offset = static_cast<usize>(index) & PAGE_MASK;
        LS_EXPECT(this->pages.size() > page_index);

        return this->pages[page_index]->elements.at(page_offset);
    }

    auto reset() -> void {
        ZoneScoped;

        this->pages.clear();
    }

    auto max_resources() -> u32 { return MAX_RESOURCE_COUNT; }
};

}  // namespace lr
