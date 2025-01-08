#pragma once

#include "Engine/Asset/UUID.hh"
#include "Engine/Graphics/Vulkan.hh"
#include "Engine/World/Model.hh"
#include "Engine/World/Scene.hh"

namespace lr {
//
//  ── ASSETS ──────────────────────────────────────────────────────────
//

struct Asset;

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
    GLB,
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

    auto write_metadata(this Asset &, const fs::path &path) -> bool;
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
    auto create_asset(AssetType type, const fs::path &path = {}) -> Asset *;
    auto create_scene(const std::string &name, const fs::path &path) -> Asset *;

    //  ── Imported Assets ─────────────────────────────────────────────────
    // Assets that already exist in the filesystem with already set UUID's
    //
    // Add already existing asset into the registry.
    // File must end with `.lrasset` extension.
    auto import_asset(const fs::path &path) -> Asset *;
    auto import_scene(Asset *asset) -> bool;

    //  ── Load Assets ─────────────────────────────────────────────────────
    // Load contents of registered assets.
    //
    auto load_model(Asset *asset) -> bool;
    auto load_texture(
        Asset *asset, vuk::Format format, vuk::Extent3D extent, ls::span<u8> pixels, const TextureSamplerInfo &sampler_info = {}) -> bool;
    auto load_texture(Asset *asset, const TextureSamplerInfo &sampler_info = {}) -> bool;
    auto load_material(Asset *asset, const Material &material_info) -> bool;
    auto load_material(Asset *asset) -> bool;
    auto load_scene(Asset *asset) -> bool;
    auto unload_scene(Asset *asset) -> void;

    //  ── Exporting Assets ────────────────────────────────────────────────
    // All export_# functions must have path, developer have freedom to
    // export their assets into custom directory if needed, otherwise
    // default to asset->path.
    //
    auto export_scene(Asset *asset, const fs::path &path) -> bool;

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
