#pragma once

#include "Engine/Core/Handle.hh"
#include "Engine/World/Scene.hh"
#include "Engine/World/WorldRenderer.hh"

namespace lr {
struct World : Handle<World> {
    static auto create(const std::string &name) -> ls::option<World>;
    static auto create_from_file(const fs::path &path) -> ls::option<World>;
    auto destroy() -> void;

    auto update_scene_data(WorldRenderer &renderer) -> bool;
    auto progress() -> void;

    auto set_active_scene(SceneID scene_id) -> void;
    auto active_scene() -> ls::option<SceneID>;

    auto should_quit() -> bool;
    auto delta_time() -> f64;

    auto component_icon(flecs::id_t id) -> Icon::detail::icon_t;
    auto create_entity(const std::string &name) -> flecs::entity;
    auto ecs() const -> const flecs::world &;
};

}  // namespace lr
