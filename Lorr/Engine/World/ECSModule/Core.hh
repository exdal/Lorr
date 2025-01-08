#pragma once

#include "Engine/Asset/UUID.hh"   // IWYU pragma: export
#include "Engine/World/Model.hh"  // IWYU pragma: export

#include <flecs.h>

template<>
struct std::formatter<flecs::string_view> : formatter<string_view> {
    template<typename FormatContext>
    constexpr auto format(const flecs::string_view &v, FormatContext &ctx) const {
        return std::format_to(ctx.out(), "{}", std::string_view(v.c_str(), v.length()));
    }
};

namespace lr::ECS {
#define ECS_EXPORT_TYPES
#include "Engine/World/ECSModule/Reflect.hh"

#include "Engine/World/ECSModule/CoreComponents.hh"
#undef ECS_EXPORT_TYPES

#undef ECS_COMPONENT_BEGIN
#undef ECS_COMPONENT_END
#undef ECS_COMPONENT_MEMBER
#undef ECS_COMPONENT_TAG

struct Core {
    Core(flecs::world &world);
};

}  // namespace lr::ECS
