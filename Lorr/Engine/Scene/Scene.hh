#pragma once

#include "Engine/Asset/UUID.hh"

#include "Engine/Memory/SlotMap.hh"

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
    ankerl::unordered_dense::map<ls::pair<UUID, usize>, std::vector<GPU::TransformID>> rendering_meshes_map = {};

    std::vector<GPU::TransformID> dirty_transforms = {};
    bool models_dirty = false;
    u32 mesh_instance_count = 0;
    u32 max_meshlet_instance_count = 0;

    GPU::CullFlags cull_flags = GPU::CullFlags::All;

public:
    auto init(this Scene &, const std::string &name) -> bool;
    auto destroy(this Scene &) -> void;

    auto import_from_file(this Scene &, const fs::path &path) -> bool;
    auto export_to_file(this Scene &, const fs::path &path) -> bool;

    auto create_entity(this Scene &, const std::string &name = {}) -> flecs::entity;
    auto delete_entity(this Scene &, flecs::entity entity) -> void;
    // clang-format off
    auto create_perspective_camera(this Scene &, const std::string &name, const glm::vec3 &position, const glm::vec3 &rotation, f32 fov, f32 aspect_ratio) -> flecs::entity;
    // clang-format on
    auto create_editor_camera(this Scene &) -> void;
    // Model = collection of meshes.
    // This function imports every mesh inside the model asset.
    // The returning entity is a parent, "model" entity where each of
    // its children contains individual meshes.
    auto create_model_entity(this Scene &, UUID &importing_model_uuid) -> flecs::entity;

    // Finds entity in root.
    auto find_entity(this Scene &, std::string_view name) -> flecs::entity;
    auto find_entity(this Scene &, u32 transform_index) -> flecs::entity;

    auto render(this Scene &, SceneRenderer &renderer, SceneRenderInfo &info) -> vuk::Value<vuk::ImageAttachment>;
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
    auto prepare_frame(this Scene &, SceneRenderer &renderer) -> PreparedFrame;

    auto add_transform(this Scene &, flecs::entity entity) -> GPU::TransformID;
    auto remove_transform(this Scene &, flecs::entity entity) -> void;

    auto attach_mesh(this Scene &, flecs::entity entity, const UUID &model_uuid, usize mesh_index) -> bool;
    auto detach_mesh(this Scene &, flecs::entity entity, const UUID &model_uuid, usize mesh_index) -> bool;

    friend AssetManager;
};

} // namespace lr
