#pragma once

namespace lr {
struct Identifier {
private:
    constexpr static usize STORAGE_SIZE = 24;
    c8 storage[STORAGE_SIZE] = {};

public:
    u64 hash = 0;

    constexpr Identifier() = default;
    template<usize N>
        requires(N <= STORAGE_SIZE - 1)
    constexpr Identifier(const c8 (&arr)[N])
        : Identifier(std::string_view(arr, N)) {}
    constexpr Identifier(std::string_view str) {
        this->hash = ankerl::unordered_dense::detail::wyhash::hash(str.data(), str.length());
        std::copy(str.begin(), str.end(), storage);
        storage[str.length()] = '\0';
    }
    constexpr std::string_view sv() const { return storage; }
    constexpr bool operator==(const Identifier &other) const { return this->hash == other.hash; };

    static Identifier random();
};
}  // namespace lr

template<>
struct ankerl::unordered_dense::hash<lr::Identifier> {
    using is_avalanching = void;
    u64 operator()(const lr::Identifier &key) const noexcept { return key.hash; }
};
