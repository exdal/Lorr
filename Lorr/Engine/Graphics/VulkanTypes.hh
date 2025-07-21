#pragma once

namespace lr {
template<usize ALIGNMENT, typename... T>
constexpr auto PushConstants_calc_size() -> usize {
    auto offset = 0_sz;
    ((offset = ls::align_up(offset, ALIGNMENT), offset += sizeof(T)), ...);
    return offset;
}

template<usize ALIGNMENT, typename... T>
constexpr auto PushConstants_calc_offsets() {
    auto offsets = std::array<usize, sizeof...(T)>{};
    auto offset = 0_sz;
    auto index = 0_sz;
    ((offsets[index++] = (offset = ls::align_up(offset, ALIGNMENT), offset), offset += sizeof(T)), ...);
    return offsets;
}

template<typename... T>
struct PushConstants {
    static_assert((std::is_trivially_copyable_v<T> && ...));
    constexpr static usize ALIGNMENT = 4;
    constexpr static usize TOTAL_SIZE = PushConstants_calc_size<ALIGNMENT, T...>();
    constexpr static auto MEMBER_OFFSETS = PushConstants_calc_offsets<ALIGNMENT, T...>();
    std::array<u8, TOTAL_SIZE> struct_data = {};

    PushConstants(T... args) {
        auto index = 0_sz;
        ((std::memcpy(struct_data.data() + MEMBER_OFFSETS[index++], &args, sizeof(T))), ...);
    }

    auto data() const -> void * {
        return struct_data.data();
    }

    auto size() const -> usize {
        return struct_data.size();
    }
};
} // namespace lr

#include <vuk/Types.hpp> // IWYU pragma: export
#include <vuk/runtime/vk/Image.hpp> // IWYU pragma: export
