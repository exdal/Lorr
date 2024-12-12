#pragma once

#define GLM_FORCE_SIMD_AVX2
#define GLM_FORCE_CXX20
#define GLM_ENABLE_EXPERIMENTAL

#include <ls/defer.hh>
#include <ls/enum.hh>
#include <ls/option.hh>
#include <ls/pair.hh>
#include <ls/range.hh>
#include <ls/result.hh>
#include <ls/span.hh>
#include <ls/static_vector.hh>
#include <ls/types.hh>

#include <tracy/Tracy.hpp>

#include <array>
#include <expected>
#include <format>
#include <filesystem>
#include <span>
#include <string_view>
#include <variant>

namespace fs = std::filesystem;

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/common.hpp>
#include <glm/mat4x4.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "Core/Logger.hh"
#include <ankerl/unordered_dense.h>
#include <plf_colony.h>

#include "Util/Icons.hh"

namespace lr {
template<typename T, usize N>
constexpr usize count_of(T (&)[N]) {
    return N;
}

template<class... T>
struct match : T... {
    using T::operator()...;
};

}  // namespace lr

template<>
struct std::formatter<fs::path> : formatter<string_view> {
    template<typename FormatContext>
    constexpr auto format(const fs::path &v, FormatContext &ctx) const {
        auto str = v.string();
        return std::format_to(ctx.out(), "{}", str);
    }
};
