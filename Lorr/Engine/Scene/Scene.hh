#pragma once

#include "Engine/Asset/UUID.hh"

#include "Engine/Scene/SceneRenderer.hh"

#include <flecs.h>

template<>
struct ankerl::unordered_dense::hash<flecs::id> {
    using is_avalanching = void;
    u64 operator()(const flecs::id &v) const noexcept { //
        return ankerl::unordered_dense::detail::wyhash::hash(&v, sizeof(flecs::id));
    }
};

template<>
struct ankerl::unordered_dense::hash<flecs::entity> {
    using is_avalanching = void;
    u64 operator()(const flecs::entity &v) const noexcept {
        return ankerl::unordered_dense::detail::wyhash::hash(&v, sizeof(flecs::entity));
    }
};

namespace lr {
struct SceneEntityDB {
    ankerl::unordered_dense::map<flecs::id, std::string_view> component_icons = {};
    std::vector<flecs::id> components = {};
    std::vector<flecs::entity> imported_modules = {};

    auto import_module(this SceneEntityDB &, flecs::entity module) -> void;
    auto is_component_known(this SceneEntityDB &, flecs::id component_id) -> bool;
    auto get_components(this SceneEntityDB &) -> ls::span<flecs::id>;
};

struct AssetManager;
enum class SceneID : u64 { Invalid = ~0_u64 };
struct Scene {
private:
    std::string name = {};
    flecs::entity root = {};
    ls::option<flecs::world> world = ls::nullopt;
    SceneEntityDB entity_db = {};
    flecs::entity editor_camera = {};

    SlotMap<GPU::Transforms, GPU::TransformID> transforms = {};
    ankerl::unordered_dense::map<flecs::entity, GPU::TransformID> entity_transforms_map = {};
    ankerl::unordered_dense::map<UUID, std::vector<GPU::TransformID>> rendering_models = {};

    std::vector<GPU::TransformID> dirty_transforms = {};
    bool models_dirty = false;

    GPU::CullFlags cull_flags = GPU::CullFlags::All;

public:
    auto init(this Scene &, const std::string &name) -> bool;
    auto destroy(this Scene &) -> void;

    auto import_from_file(this Scene &, const fs::path &path) -> bool;
    auto export_to_file(this Scene &, const fs::path &path) -> bool;

    auto create_entity(this Scene &, const std::string &name = {}) -> flecs::entity;
    auto
    create_perspective_camera(this Scene &, const std::string &name, const glm::vec3 &position, const glm::vec3 &rotation, f32 fov, f32 aspect_ratio)
        -> flecs::entity;
    auto create_editor_camera(this Scene &) -> void;

    // Finds entity in root.
    auto find_entity(this Scene &, std::string_view name) -> flecs::entity;

    auto render(this Scene &, SceneRenderer &renderer, const vuk::Extent3D &extent, vuk::Format format) -> vuk::Value<vuk::ImageAttachment>;
    auto tick(this Scene &, f32 delta_time) -> bool;

    auto set_name(this Scene &, const std::string &name) -> void;
    auto set_dirty(this Scene &, flecs::entity entity) -> void;

    auto get_root(this Scene &) -> flecs::entity;
    auto get_world(this Scene &) -> flecs::world &;
    auto get_editor_camera(this Scene &) -> flecs::entity;
    auto get_name(this Scene &) -> const std::string &;
    auto get_name_sv(this Scene &) -> std::string_view;
    auto get_entity_db(this Scene &) -> SceneEntityDB &;
    auto get_cull_flags(this Scene &) -> GPU::CullFlags &;

private:
    auto compose(this Scene &) -> SceneComposeInfo;

    auto add_transform(this Scene &, flecs::entity entity) -> GPU::TransformID;
    auto remove_transform(this Scene &, flecs::entity entity) -> void;

    auto attach_model(this Scene &, flecs::entity entity, const UUID &model_uuid) -> bool;
    auto detach_model(this Scene &, flecs::entity entity, const UUID &model_uuid) -> bool;

    friend AssetManager;
};

} // namespace lr
