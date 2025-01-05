#pragma once

#include "Engine/World/Scene.hh"

namespace lr {
struct World {
    auto init(this World &) -> bool;
    auto destroy(this World &) -> void;

    auto set_active_scene(this World &, SceneID scene_id) -> void;
    auto get_active_scene(this World &) -> ls::option<SceneID>;

    auto component_icon(this World &, flecs::id_t id) -> Icon::detail::icon_t;

private:
    ls::option<SceneID> active_scene = ls::nullopt;
    ankerl::unordered_dense::map<flecs::id_t, Icon::detail::icon_t> component_icons = {};
};

}  // namespace lr
