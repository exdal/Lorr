#include "Engine/Asset/UUID.hh"

#include <algorithm>
#include <random>

namespace lr {
thread_local std::random_device random_device;
thread_local std::mt19937_64 random_engine(random_device());
thread_local std::uniform_int_distribution<u64> uniform_dist;

constexpr auto is_hex_digit(c8 c) -> bool {
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}

constexpr auto hex_to_u8(c8 c) -> u8 {
    LS_EXPECT(is_hex_digit(c));

    if (c >= '0' && c <= '9') {
        return static_cast<u8>(c - '0');
    }

    return (c >= 'A' && c <= 'F') ? static_cast<u8>(c - 'A' + 10) : static_cast<u8>(c - 'a' + 10);
}

auto UUID::generate_random() -> UUID {
    ZoneScoped;

    UUID uuid;
    std::ranges::generate(uuid.data._u8_arr, std::ref(random_device));
    uuid.data._u64[0] &= 0xffffffffffff0fff_u64;
    uuid.data._u64[0] |= 0x0000000000004000_u64;
    uuid.data._u64[1] &= 0x3fffffffffffffff_u64;
    uuid.data._u64[1] |= 0x8000000000000000_u64;

    return uuid;
}

auto UUID::from_string(std::string_view str) -> ls::option<UUID> {
    ZoneScoped;

    if (str.size() != STRING_LENGTH || str[8] != '-' || str[13] != '-' || str[18] != '-' || str[23] != '-') {
        return ls::nullopt;
    }

    auto convert_segment = [](std::string_view target, usize offset, usize length) -> ls::option<u64> {
        u64 val = 0;
        for (usize i = 0; i < length; i++) {
            auto c = target[offset + i];
            if (!is_hex_digit(c)) {
                return ls::nullopt;
            }

            val = (val << 4) | hex_to_u8(c);
        }

        return val;
    };

    UUID uuid;
    try {
        uuid.data._u64[0] = (convert_segment(str, 0, 8).value() << 32) | //
            (convert_segment(str, 9, 4).value() << 16) | //
            convert_segment(str, 14, 4).value();
        uuid.data._u64[1] = (convert_segment(str, 19, 4).value() << 48) | //
            convert_segment(str, 24, 12).value();

#ifdef LS_DEBUG
        uuid.debug = uuid.str();
#endif
    } catch (std::exception &) {
        return ls::nullopt;
    }

    return uuid;
}

auto UUID::str() const -> std::string {
    ZoneScoped;

    return fmt::format(
        "{:08x}-{:04x}-{:04x}-{:04x}-{:012x}",
        static_cast<u32>(this->data._u64[0] >> 32_u64),
        static_cast<u32>((this->data._u64[0] >> 16_u64) & 0x0000ffff_u64),
        static_cast<u32>(this->data._u64[0] & 0x0000ffff_u64),
        static_cast<u32>(this->data._u64[1] >> 48_u64),
        this->data._u64[1] & 0x0000ffffffffffff_u64
    );
}

} // namespace lr
