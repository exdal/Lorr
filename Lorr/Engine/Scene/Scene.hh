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
struct AssetManager;
enum class SceneID : u64 { Invalid = ~0_u64 };
struct Scene {
private:
    std::string name = {};
    flecs::entity root = {};
    ls::option<flecs::world> world = ls::nullopt;
    std::vector<flecs::id> known_component_ids = {};

    SlotMap<GPU::Transforms, GPU::TransformID> transforms = {};
    ankerl::unordered_dense::map<flecs::entity, GPU::TransformID> entity_transforms_map = {};
    ankerl::unordered_dense::map<ls::pair<UUID, usize>, std::vector<GPU::TransformID>> rendering_meshes_map = {};
    std::vector<GPU::TransformID> dirty_transforms = {};

    std::vector<GPU::Material> gpu_materials = {};

    bool models_dirty = false;
    u32 mesh_instance_count = 0;
    u32 max_meshlet_instance_count = 0;

    u32 cull_flags = GPU::CullFlags::All;

    GPU::Atmosphere last_atmosphere = {};

public:
    auto init(this Scene &, const std::string &name) -> bool;
    auto destroy(this Scene &) -> void;

    template<typename T>
    auto import_module(this Scene &self) -> void {
        ZoneScoped;

        return self.import_module(self.world->import <T>());
    }
    auto import_module(this Scene &, flecs::entity module_entity) -> void;
    auto is_component_known(this Scene &, flecs::id component_id) -> bool;

    auto import_from_file(this Scene &, const fs::path &path) -> bool;
    auto export_to_file(this Scene &, const fs::path &path) -> bool;

    auto create_entity(this Scene &, const std::string &name = {}) -> flecs::entity;
    auto delete_entity(this Scene &, flecs::entity entity) -> void;
    auto create_perspective_camera(this Scene &, const std::string &name, const glm::vec3 &position, f32 yaw, f32 pitch, f32 fov) -> flecs::entity;
    // Model = collection of meshes.
    // This function imports every mesh inside the model asset.
    // The returning entity is a parent, "model" entity where each of
    // its children contains individual meshes.
    auto create_model_entity(this Scene &, UUID &importing_model_uuid) -> flecs::entity;

    // Finds entity in root.
    auto find_entity(this Scene &, std::string_view name) -> flecs::entity;
    auto find_entity(this Scene &, u32 transform_index) -> flecs::entity;

    // If we really want to render something, camera needs to be there
    auto prepare_frame(this Scene &, SceneRenderer &renderer, u32 image_count, ls::option<GPU::Camera> override_camera = ls::nullopt)
        -> PreparedFrame;
    auto tick(this Scene &, f32 delta_time) -> bool;

    auto set_name(this Scene &, const std::string &name) -> void;
    auto set_dirty(this Scene &, flecs::entity entity) -> void;

    auto get_root(this Scene &) -> flecs::entity;
    auto get_world(this Scene &) -> flecs::world &;
    auto get_name(this Scene &) -> const std::string &;
    auto get_name_sv(this Scene &) -> std::string_view;
    auto get_cull_flags(this Scene &) -> u32 &;

    auto get_known_component_ids(this Scene &self) -> auto {
        return ls::span(self.known_component_ids);
    }

private:
    auto add_transform(this Scene &, flecs::entity entity) -> GPU::TransformID;
    auto remove_transform(this Scene &, flecs::entity entity) -> void;

    auto attach_mesh(this Scene &, flecs::entity entity, const UUID &model_uuid, usize mesh_index) -> bool;
    auto detach_mesh(this Scene &, flecs::entity entity, const UUID &model_uuid, usize mesh_index) -> bool;

    friend AssetManager;
};

} // namespace lr
