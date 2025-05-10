#pragma once

namespace lr {
struct UUID {
private:
    union {
        u64 _u64[2] = {};
        u8 _u8[16];
        std::array<u8, 16> _u8_arr;
    } data = {};

#ifdef LS_DEBUG
    std::string debug = {};
#endif

public:
    constexpr static usize STRING_LENGTH = 36;

    static auto generate_random() -> UUID;
    static auto from_string(std::string_view str) -> ls::option<UUID>;

    auto str() const -> std::string;

    UUID() = default;
    explicit UUID(std::nullptr_t) {};
    UUID(const UUID &other) = default;
    UUID &operator=(const UUID &other) = default;
    UUID(UUID &&other) = default;
    UUID &operator=(UUID &&other) = default;

    auto bytes() const -> const std::array<u8, 16> {
        return data._u8_arr;
    }

    auto bytes() -> const std::array<u8, 16> {
        return data._u8_arr;
    }

    constexpr auto operator==(const UUID &other) const -> bool {
        return this->data._u64[0] == other.data._u64[0] && this->data._u64[1] == other.data._u64[1];
    }

    explicit operator bool() const {
        return data._u64[0] != 0 && data._u64[1] != 0;
    }
};
} // namespace lr

template<>
struct ankerl::unordered_dense::hash<lr::UUID> {
    using is_avalanching = void;
    u64 operator()(const lr::UUID &uuid) const noexcept {
        const auto &v = uuid.bytes();
        return ankerl::unordered_dense::detail::wyhash::hash(v.data(), ls::size_bytes(v));
    }
};
