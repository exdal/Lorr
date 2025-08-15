#pragma once

#include "Engine/Asset/AssetFile.hh"
#include "Engine/Asset/Model.hh"
#include "Engine/Asset/UUID.hh"

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

using AssetRegistry = ankerl::unordered_dense::map<UUID, Asset>;
struct AssetManager {
    constexpr static auto MODULE_NAME = "Asset Manager";

    fs::path root_path = fs::current_path();
    AssetRegistry registry = {};

    std::shared_mutex registry_mutex = {};
    SlotMap<Model, ModelID> models = {};

    std::shared_mutex textures_mutex = {};
    SlotMap<Texture, TextureID> textures = {};

    std::shared_mutex materials_mutex = {};
    SlotMap<Material, MaterialID> materials = {};
    std::vector<MaterialID> dirty_materials = {};

    SlotMap<std::unique_ptr<Scene>, SceneID> scenes = {};

    auto init(this AssetManager &) -> bool;
    auto destroy(this AssetManager &) -> void;

    auto asset_root_path(this AssetManager &, AssetType type) -> fs::path;
    auto to_asset_file_type(this AssetManager &, const fs::path &path) -> AssetFileType;
    auto to_asset_type_sv(this AssetManager &, AssetType type) -> std::string_view;
    auto get_registry(this AssetManager &) -> const AssetRegistry &;

    //  ── Created Assets ──────────────────────────────────────────────────
    // Assets that will be created and asinged new UUID. New `.lrasset` file
    // will be written in the same directory as target asset.
    // All created assets will be automatically registered into the registry.
    //
    auto create_asset(this AssetManager &, AssetType type, const fs::path &path = {}) -> UUID;
    auto init_new_scene(this AssetManager &, const UUID &uuid, const std::string &name) -> bool;

    //  ── Imported Assets ─────────────────────────────────────────────────
    // If a valid meta file exists in the same path as importing asset, this
    // function will act like `register_asset`. Otherwise this function will
    // act like `create_asset` which creates new unique handle to asset with
    // meta file in the same path.
    auto import_asset(this AssetManager &, const fs::path &path) -> UUID;
    auto import_project(this AssetManager &, const fs::path &path) -> void;

    //  ── Registered Assets ───────────────────────────────────────────────
    // Assets that already exist in project root and have meta file with
    // valid UUID's.
    //
    // Add already existing asset into the registry.
    // File must end with `.lrasset` extension.
    auto register_asset(this AssetManager &, const fs::path &path) -> UUID;
    auto register_asset(this AssetManager &, const UUID &uuid, AssetType type, const fs::path &path) -> bool;

    //  ── Load Assets ─────────────────────────────────────────────────────
    // Load contents of registered assets.
    //
    auto load_asset(this AssetManager &, const UUID &uuid) -> bool;
    auto unload_asset(this AssetManager &, const UUID &uuid) -> bool;

    auto load_model(this AssetManager &, const UUID &uuid) -> bool;
    auto unload_model(this AssetManager &, const UUID &uuid) -> bool;

    auto load_texture(this AssetManager &, const UUID &uuid, const TextureInfo &info = {}) -> bool;
    auto unload_texture(this AssetManager &, const UUID &uuid) -> bool;
    auto is_texture_loaded(this AssetManager &, const UUID &uuid) -> bool;

    auto load_material(this AssetManager &, const UUID &uuid, const MaterialInfo &info) -> bool;
    auto unload_material(this AssetManager &, const UUID &uuid) -> bool;
    auto is_material_loaded(this AssetManager &, const UUID &uuid) -> bool;

    auto load_scene(this AssetManager &, const UUID &uuid) -> bool;
    auto unload_scene(this AssetManager &, const UUID &uuid) -> bool;

    //  ── Exporting Assets ────────────────────────────────────────────────
    // All export_# functions must have path, developer have freedom to
    // export their assets into custom directory if needed, otherwise
    // default to asset->path.
    //
    auto export_asset(this AssetManager &, const UUID &uuid, const fs::path &path) -> bool;
    auto export_texture(this AssetManager &, const UUID &uuid, JsonWriter &json, const fs::path &path) -> bool;
    auto export_model(this AssetManager &, const UUID &uuid, JsonWriter &json, const fs::path &path) -> bool;
    auto export_scene(this AssetManager &, const UUID &uuid, JsonWriter &json, const fs::path &path) -> bool;

    auto delete_asset(this AssetManager &, const UUID &uuid) -> void;

    auto get_asset(this AssetManager &, const UUID &uuid) -> Asset *;
    auto get_model(this AssetManager &, const UUID &uuid) -> Model *;
    auto get_model(this AssetManager &, ModelID model_id) -> Model *;
    auto get_texture(this AssetManager &, const UUID &uuid) -> Texture *;
    auto get_texture(this AssetManager &, TextureID texture_id) -> Texture *;
    auto get_material(this AssetManager &, const UUID &uuid) -> Material *;
    auto get_material(this AssetManager &, MaterialID material_id) -> Material *;
    auto get_scene(this AssetManager &, const UUID &uuid) -> Scene *;
    auto get_scene(this AssetManager &, SceneID scene_id) -> Scene *;

    auto set_material_dirty(this AssetManager &, MaterialID material_id) -> void;
    auto get_dirty_material_ids(this AssetManager &) -> std::vector<MaterialID>;
};
} // namespace lr
