#pragma once

#define GLM_FORCE_SIMD_AVX2
#define GLM_FORCE_CXX20
#define GLM_ENABLE_EXPERIMENTAL

#include <ls/defer.hh>
#include <ls/enum.hh>
#include <ls/option.hh>
#include <ls/pair.hh>
#include <ls/span.hh>
#include <ls/static_vector.hh>
#include <ls/types.hh>

#include <tracy/Tracy.hpp>

#include <array>
#include <deque>
#include <expected>
#include <filesystem>
#include <format>
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

#include "Engine/Math/Quat.hh"
#include "Engine/Math/Rotation.hh"

#include "Core/Logger.hh"
#include <ankerl/unordered_dense.h>
#include <plf_colony.h>

template<>
struct std::formatter<fs::path> : formatter<string_view> {
    template<typename FormatContext>
    constexpr auto format(const fs::path &v, FormatContext &ctx) const {
        auto str = v.string();
        return std::format_to(ctx.out(), "{}", str);
    }
};

template<typename T, typename U>
struct ankerl::unordered_dense::hash<ls::pair<T, U>> {
    using is_avalanching = void;
    u64 operator()(const ls::pair<T, U> &v) const noexcept {
        usize seed = 0;
        auto lhs = ankerl::unordered_dense::hash<T>{}(v.n0);
        ls::hash_combine(seed, lhs);
        auto rhs = ankerl::unordered_dense::hash<U>{}(v.n1);
        ls::hash_combine(seed, rhs);

        return seed;
    }
};

#define LR_CALLSTACK std::source_location LOC
#define LR_THISCALL LR_CALLSTACK = std::source_location::current()
