#pragma once

#include "Util/Types.hh"

#include <tracy/Tracy.hpp>

#include <array>
#include <span>
#include <string_view>
#include <optional>

#include <glm/trigonometric.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "Core/Log.hh"
#include "Util/Result.hh"
#include "Util/TypeOperators.hh"
#include "Util/span.hh"
#include "Util/static_vector.hh"
#include <plf_colony.h>

namespace lr {
template<typename T, usize N>
constexpr usize count_of(T (&)[N])
{
    return N;
}

template<typename T>
T *temp_v(T &&v)
{
    return &v;
}

}  // namespace lr
