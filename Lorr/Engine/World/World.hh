#pragma once

#include "Engine/Core/Handle.hh"
#include "Engine/World/Scene.hh"
#include "Engine/World/WorldRenderer.hh"

namespace lr {
struct World : Handle<World> {
    static auto create(const std::string &name) -> ls::option<World>;
    static auto create_from(const fs::path &path) -> ls::option<World>;
    auto destroy() -> void;

    auto export_to(const fs::path &path) -> bool;

    auto begin_frame(WorldRenderer &renderer) -> void;
    auto end_frame() -> void;

    template<typename T>
    auto add_scene(const std::string &name) -> ls::option<SceneID> {
        ZoneScoped;

        std::unique_ptr<Scene> scene = std::make_unique<T>(name, *this);
        return this->add_scene(std::move(scene));
    }

    auto add_scene(std::unique_ptr<Scene> &&scene) -> ls::option<SceneID>;
    auto set_active_scene(SceneID scene_id) -> void;
    auto scene(SceneID scene_id) -> Scene &;
    auto active_scene() -> ls::option<SceneID>;

    auto should_quit() -> bool;
    auto delta_time() -> f64;

    auto component_icon(flecs::id_t id) -> Icon::detail::icon_t;
    auto create_entity(const std::string &name) -> flecs::entity;
    auto ecs() const -> const flecs::world &;
};

}  // namespace lr
