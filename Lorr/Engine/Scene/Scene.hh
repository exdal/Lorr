#pragma once

#include "Engine/Scene/SceneRenderer.hh"

#include <flecs.h>

namespace lr {
struct AssetManager;

enum class SceneID : u64 { Invalid = ~0_u64 };
struct Scene {
    auto init(this Scene &, const std::string &name) -> bool;
    auto destroy(this Scene &) -> void;

    auto import_from_file(this Scene &, const fs::path &path) -> bool;
    auto export_to_file(this Scene &, const fs::path &path) -> bool;

    auto create_entity(this Scene &, const std::string &name = {}) -> flecs::entity;
    auto create_perspective_camera(
        this Scene &, const std::string &name, const glm::vec3 &position, const glm::vec3 &rotation, f32 fov, f32 aspect_ratio)
        -> flecs::entity;
    auto create_editor_camera(this Scene &) -> void;

    auto upload_scene(this Scene &, SceneRenderer &renderer) -> void;
    auto tick(this Scene &) -> bool;

    auto set_name(this Scene &, const std::string &name) -> void;

    auto get_root(this Scene &) -> flecs::entity;
    auto get_world(this Scene &) -> flecs::world &;
    auto editor_camera(this Scene &) -> flecs::entity;
    auto get_name(this Scene &) -> const std::string &;
    auto get_name_sv(this Scene &) -> std::string_view;
    auto get_imported_modules(this Scene &) -> ls::span<flecs::entity>;

private:
    std::string name = {};
    flecs::entity root = {};
    ls::option<flecs::world> world = ls::nullopt;

    std::vector<flecs::entity> imported_modules = {};
    GPUEntityID editor_camera_id = GPUEntityID::Invalid;
    SlotMap<flecs::entity, GPUEntityID> cameras = {};

    friend AssetManager;
};

}  // namespace lr
