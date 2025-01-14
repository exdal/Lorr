#pragma once

#include "Engine/Asset/Model.hh"
#include "Engine/Asset/UUID.hh"

#include "Engine/Graphics/Vulkan.hh"

#include "Engine/Util/JsonWriter.hh"

#include "Engine/World/Scene.hh"

namespace lr {
enum class AssetType : u32 {
    None = 0,
    Root = 0,
    Shader,
    Model,
    Texture,
    Material,
    Font,
    Scene,
};

// List of file extensions supported by Engine.
enum class AssetFileType : u32 {
    None = 0,
    Binary,
    Meta,
    GLB,
    GLTF,
    PNG,
    JPEG,
    JSON,
};

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
};

//
//  ── ASSET FILE ──────────────────────────────────────────────────────
//

enum class AssetFileFlags : u64 {
    None = 0,
};
consteval void enable_bitmask(AssetFileFlags);

struct TextureAssetFileHeader {
    vuk::Extent3D extent = {};
    vuk::Format format = vuk::Format::eUndefined;
};

struct AssetFileHeader {
    c8 magic[4] = { 'L', 'O', 'R', 'R' };
    u16 version = 1;
    AssetFileFlags flags = AssetFileFlags::None;
    AssetType type = AssetType::None;
    union {
        TextureAssetFileHeader texture_header = {};
    };
};

using AssetRegistry = ankerl::unordered_dense::map<UUID, Asset>;
struct AssetManager : Handle<AssetManager> {
    static auto create(Device *device) -> AssetManager;
    auto destroy() -> void;

    auto asset_root_path(AssetType type) -> fs::path;
    auto to_asset_file_type(const fs::path &path) -> AssetFileType;
    auto material_buffer() const -> Buffer &;
    auto registry() const -> const AssetRegistry &;

    //  ── Created Assets ──────────────────────────────────────────────────
    // Assets that will be created and asinged new UUID. New `.lrasset` file
    // will be written in the same directory as target asset.
    // All created assets will be automatically registered into the registry.
    //
    auto create_asset(AssetType type, const fs::path &path = {}) -> UUID;
    auto init_new_model(const UUID &uuid) -> bool;
    auto init_new_scene(const UUID &uuid, const std::string &name) -> bool;

    //  ── Imported Assets ─────────────────────────────────────────────────
    // Assets that already exist in the filesystem with already set UUID's
    //
    // Add already existing asset into the registry.
    // File must end with `.lrasset` extension.
    auto import_asset(const fs::path &path) -> UUID;
    auto import_asset(const UUID &uuid, AssetType type, const fs::path &path) -> bool;

    //  ── Load Assets ─────────────────────────────────────────────────────
    // Load contents of registered assets.
    //
    auto load_model(const UUID &uuid) -> bool;
    auto load_texture(const UUID &uuid, ls::span<u8> pixels, const TextureSamplerInfo &sampler_info = {}) -> bool;
    auto load_texture(const UUID &uuid, const TextureSamplerInfo &sampler_info = {}) -> bool;
    auto load_material(const UUID &uuid, const Material &material_info) -> bool;
    auto load_material(const UUID &uuid) -> bool;

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

    auto get_asset(const UUID &uuid) -> Asset *;
    auto get_model(const UUID &uuid) -> Model *;
    auto get_model(ModelID model_id) -> Model *;
    auto get_texture(const UUID &uuid) -> Texture *;
    auto get_texture(TextureID texture_id) -> Texture *;
    auto get_material(const UUID &uuid) -> Material *;
    auto get_material(MaterialID material_id) -> Material *;
    auto get_scene(const UUID &uuid) -> Scene *;
    auto get_scene(SceneID scene_id) -> Scene *;
};
}  // namespace lr
