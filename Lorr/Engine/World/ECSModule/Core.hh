#pragma once

#include "Engine/Asset/UUID.hh"   // IWYU pragma: export
#include "Engine/World/Model.hh"  // IWYU pragma: export

#include <flecs.h>

namespace lr::ECS {
#define ECS_EXPORT_TYPES
#include "Engine/World/ECSModule/Reflect.hh"

#include "Engine/World/ECSModule/CoreComponents.hh"
#undef ECS_EXPORT_TYPES

struct Core {
    Core(flecs::world &world);
};

}  // namespace lr::ECS
