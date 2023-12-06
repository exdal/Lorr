#pragma once

#include "STL/Types.hh"

#include <tracy/Tracy.hpp>

#include <EASTL/array.h>
#include <EASTL/fixed_vector.h>
#include <EASTL/span.h>
#include <EASTL/string_view.h>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "Core/Logger/Format.hh"
#include "Core/Logger/Logger.hh"
#include "Utils/TypeOperators.hh"

template<typename T, usize SizeT>
using fixed_vector = eastl::fixed_vector<T, SizeT, false>;