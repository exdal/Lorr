#pragma once

#include "Engine/Asset/AssetFile.hh"
#include "Engine/Asset/Model.hh"
#include "Engine/Asset/UUID.hh"

#include "Engine/Core/Handle.hh"

#include "Engine/Util/JsonWriter.hh"

#include "Engine/Scene/Scene.hh"

namespace lr {
struct Asset {
    UUID uuid = {};
    fs::path path = {};
    AssetType type = AssetType::None;
    union {
        ModelID model_id = ModelID::Invalid;
        TextureID texture_id;
        MaterialID material_id;
        SceneID scene_id;
    };

    // Reference count of loads
    u64 ref_count = 0;

    auto is_loaded() const -> bool {
        return model_id != ModelID::Invalid;
    }

    auto acquire_ref() -> void {
        ++std::atomic_ref(ref_count);
    }

    auto release_ref() -> bool {
        return (--std::atomic_ref(ref_count)) == 0;
    }
};

//
//  ── ASSET FILE ──────────────────────────────────────────────────────
//

using AssetRegistry = ankerl::unordered_dense::map<UUID, Asset>;
struct AssetManager : Handle<AssetManager> {
    static auto create(Device *device) -> AssetManager;
    auto destroy() -> void;

    auto asset_root_path(AssetType type) -> fs::path;
    auto to_asset_file_type(const fs::path &path) -> AssetFileType;
    auto to_asset_type_sv(AssetType type) -> std::string_view;
    auto registry() const -> const AssetRegistry &;

    //  ── Created Assets ──────────────────────────────────────────────────
    // Assets that will be created and asinged new UUID. New `.lrasset` file
    // will be written in the same directory as target asset.
    // All created assets will be automatically registered into the registry.
    //
    auto create_asset(AssetType type, const fs::path &path = {}) -> UUID;
    auto init_new_scene(const UUID &uuid, const std::string &name) -> bool;

    //  ── Imported Assets ─────────────────────────────────────────────────
    auto import_asset(const fs::path &path) -> UUID;

    //  ── Registered Assets ─────────────────────────────────────────────────
    // Assets that already exist in project root and have meta file with
    // valid UUID's.
    //
    // Add already existing asset into the registry.
    // File must end with `.lrasset` extension.
    auto register_asset(const fs::path &path) -> UUID;
    auto register_asset(const UUID &uuid, AssetType type, const fs::path &path) -> bool;

    //  ── Load Assets ─────────────────────────────────────────────────────
    // Load contents of registered assets.
    //
    auto load_asset(const UUID &uuid) -> bool;
    auto unload_asset(const UUID &uuid) -> void;

    auto load_model(const UUID &uuid) -> bool;
    auto unload_model(const UUID &uuid) -> void;

    auto load_texture(const UUID &uuid, const TextureInfo &info = {}) -> bool;
    auto unload_texture(const UUID &uuid) -> void;
    auto is_texture_loaded(const UUID &uuid) -> bool;

    auto load_material(const UUID &uuid, const Material &material_info) -> bool;
    auto unload_material(const UUID &uuid) -> void;
    auto is_material_loaded(const UUID &uuid) -> bool;

    auto load_scene(const UUID &uuid) -> bool;
    auto unload_scene(const UUID &uuid) -> void;

    //  ── Exporting Assets ────────────────────────────────────────────────
    // All export_# functions must have path, developer have freedom to
    // export their assets into custom directory if needed, otherwise
    // default to asset->path.
    //
    auto export_asset(const UUID &uuid, const fs::path &path) -> bool;
    auto export_texture(const UUID &uuid, JsonWriter &json, const fs::path &path) -> bool;
    auto export_model(const UUID &uuid, JsonWriter &json, const fs::path &path) -> bool;
    auto export_scene(const UUID &uuid, JsonWriter &json, const fs::path &path) -> bool;

    auto delete_asset(const UUID &uuid) -> void;

    auto get_asset(const UUID &uuid) -> Asset *;
    auto get_model(const UUID &uuid) -> Model *;
    auto get_model(ModelID model_id) -> Model *;
    auto get_texture(const UUID &uuid) -> Texture *;
    auto get_texture(TextureID texture_id) -> Texture *;
    auto get_material(const UUID &uuid) -> Material *;
    auto get_material(MaterialID material_id) -> Material *;
    auto get_scene(const UUID &uuid) -> Scene *;
    auto get_scene(SceneID scene_id) -> Scene *;

    auto set_material_dirty(MaterialID material_id) -> void;
    auto get_materials_buffer() -> vuk::Value<vuk::Buffer>;
    auto get_materials_descriptor_set() -> vuk::PersistentDescriptorSet *;

private:
    auto begin_asset_meta(JsonWriter &json, const UUID &uuid, AssetType type) -> void;
    auto write_model_asset_meta(JsonWriter &json, ls::span<UUID> material_uuids, ls::span<Material> materials) -> bool;
    auto write_texture_asset_meta(JsonWriter &json, Texture *texture) -> bool;
    auto write_material_asset_meta(JsonWriter &json, Material *material) -> bool;
    auto write_scene_asset_meta(JsonWriter &json, Scene *scene) -> bool;
    auto end_asset_meta(JsonWriter &json, const fs::path &path) -> bool;
};
} // namespace lr
