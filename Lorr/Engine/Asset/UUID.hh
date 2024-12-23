#pragma once

namespace lr {
struct UUID {
private:
    union {
        u64 data_u64[2] = {};
        u8 data_u8[16];
        std::array<u8, 16> data_u8_arr;
    };

public:
    constexpr static usize STRING_LENGTH = 36;

    static auto generate_random() -> UUID;
    static auto from_string(std::string_view str) -> ls::option<UUID>;

    auto str() const -> std::string;

    UUID() = default;
    UUID(const UUID &other) = default;
    UUID &operator=(const UUID &other) = default;
    UUID(UUID &&other) = default;
    UUID &operator=(UUID &&other) = default;

    constexpr auto operator==(const UUID &other) const -> bool {
        return this->data_u64[0] == other.data_u64[0] && this->data_u64[1] == other.data_u64[1];
    }
};
}  // namespace lr

template<>
struct ankerl::unordered_dense::hash<lr::UUID> {
    using is_avalanching = void;
    u64 operator()(const lr::UUID &uuid) const noexcept { return ankerl::unordered_dense::detail::wyhash::hash(&uuid, sizeof(lr::UUID)); }
};
