#pragma once

#include "Engine/Core/Handle.hh"

#include <flecs.h>

namespace lr {
using World_H = Handle<struct World>;
struct AssetManager;

enum class SceneID : u32 { Invalid = ~0_u32 };
struct Scene : Handle<Scene> {
    static auto create(const std::string &name, World *world) -> ls::option<Scene>;
    auto destroy() -> void;

    auto create_entity(const std::string &name) -> flecs::entity;
    auto create_perspective_camera(const std::string &name, const glm::vec3 &position, const glm::vec3 &rotation, f32 fov, f32 aspect_ratio)
        -> u32;

    auto root() -> flecs::entity;
    auto cameras() -> ls::span<flecs::entity>;
    auto editor_camera_index() -> u32;
    auto name() -> std::string;
    auto name_sv() -> std::string_view;

    friend AssetManager;
};

}  // namespace lr
