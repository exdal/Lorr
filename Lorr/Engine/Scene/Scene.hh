#pragma once

#include "Engine/Scene/SceneRenderer.hh"

#include <flecs.h>

template<>
struct ankerl::unordered_dense::hash<flecs::id> {
    using is_avalanching = void;
    u64 operator()(const flecs::id &v) const noexcept {  //
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
    ankerl::unordered_dense::map<flecs::id, Icon::detail::icon_t> component_icons = {};
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

    GPUEntityID editor_camera_id = GPUEntityID::Invalid;

    // We need additional map to avoid using components to access GPUEntityID of a given entity
    SlotMap<flecs::entity, GPUEntityID> gpu_entities = {};                             // GPUEntityID -> flecs::entity
    ankerl::unordered_dense::map<flecs::entity, GPUEntityID> gpu_entities_remap = {};  // flecs::entity -> GPUEntityID
    std::vector<GPUEntityID> dirty_entities = {};

    friend AssetManager;

public:
    auto init(this Scene &, const std::string &name) -> bool;
    auto destroy(this Scene &) -> void;

    auto import_from_file(this Scene &, const fs::path &path) -> bool;
    auto export_to_file(this Scene &, const fs::path &path) -> bool;

    auto create_entity(this Scene &, const std::string &name = {}) -> flecs::entity;
    auto create_perspective_camera(
        this Scene &, const std::string &name, const glm::vec3 &position, const glm::vec3 &rotation, f32 fov, f32 aspect_ratio)
        -> flecs::entity;
    auto create_editor_camera(this Scene &) -> void;

    auto render(this Scene &, SceneRenderer &renderer, const vuk::Extent3D &extent, vuk::Format format) -> vuk::Value<vuk::ImageAttachment>;
    auto tick(this Scene &) -> bool;

    auto set_name(this Scene &, const std::string &name) -> void;
    auto set_dirty(this Scene &, GPUEntityID gpu_entity_id) -> void;

    auto get_root(this Scene &) -> flecs::entity;
    auto get_world(this Scene &) -> flecs::world &;
    auto editor_camera(this Scene &) -> flecs::entity;
    auto get_name(this Scene &) -> const std::string &;
    auto get_name_sv(this Scene &) -> std::string_view;
    auto get_gpu_entity(this Scene &, flecs::entity entity) -> ls::option<GPUEntityID>;
    auto get_entity_db(this Scene &) -> SceneEntityDB &;

private:
    auto add_gpu_entity(this Scene &, flecs::entity entity) -> GPUEntityID;
    auto remove_gpu_entity(this Scene &, GPUEntityID id) -> void;
};

}  // namespace lr
