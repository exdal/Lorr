#pragma once

#define GLM_ENABLE_EXPERIMENTAL

#include <ls/enum.hh>
#include <ls/option.hh>
#include <ls/path.hh>
#include <ls/range.hh>
#include <ls/result.hh>
#include <ls/span.hh>
#include <ls/static_vector.hh>
#include <ls/types.hh>

#include <tracy/Tracy.hpp>

#include <array>
#include <filesystem>
#include <span>
#include <string_view>

#include <glm/mat4x4.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "Core/Log.hh"
#include <plf_colony.h>

namespace fs = std::filesystem;

namespace lr {
template<typename T, usize N>
constexpr usize count_of(T (&)[N]) {
    return N;
}

}  // namespace lr
