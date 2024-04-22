#pragma once

#include "Util/Types.hh"

#include <tracy/Tracy.hpp>

#include <array>
#include <span>
#include <string_view>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "Core/Log.hh"
#include "Util/Handle.hh"
#include "Util/Result.hh"
#include "Util/TypeOperators.hh"
#include "Util/static_vector.hh"
#include <ankerl/unordered_dense.h>
#include <plf_colony.h>

namespace lr {
template<typename T, usize N>
constexpr usize count_of(T (&_)[N])
{
    return N;
}
}  // namespace lr
