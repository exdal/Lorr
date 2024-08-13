#pragma once

#include "Engine/Memory/Hasher.hh"

namespace lr {
struct Identifier {
    constexpr static std::string_view SPLIT = "://";
    template<usize N>
    constexpr Identifier(const c8 (&arr)[N]) {
        std::string_view str = arr;
        usize split_pos = str.find(SPLIT);
        LR_ASSERT(split_pos != ~0_sz, "Identifiers must be formated like 'category://name'!");

        this->category_hash = fnv64_str(str.substr(0, split_pos));
        this->name_hash = fnv64_str(str.substr(split_pos + SPLIT.length()));
    }

    constexpr Identifier(std::string_view str) {
        usize split_pos = str.find(SPLIT);
        LR_ASSERT(split_pos != ~0_sz, "Identifiers must be formated like 'category://name'!");

        this->category_hash = fnv64_str(str.substr(0, split_pos));
        this->name_hash = fnv64_str(str.substr(split_pos + SPLIT.length()));
    }

    u64 category_hash = 0;
    u64 name_hash = 0;
};
}  // namespace lr

template<>
struct std::hash<lr::Identifier> {
    usize operator()(const lr::Identifier &v) const noexcept { return v.name_hash ^ (v.category_hash << 1); }
};
