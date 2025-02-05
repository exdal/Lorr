#pragma once

namespace lr::Math {
// Normalize a vector in degrees form to [-180, 180]
template<glm::length_t N, typename T>
constexpr auto normalize_180(const glm::vec<N, T> &rot) -> glm::vec<N, T> {
    return glm::mod(rot + 180.0f, glm::vec<N, T>(360.0f)) - 180.0f;
}
}  // namespace lr::Math
