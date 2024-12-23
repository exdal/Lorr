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
};

//
//  ── ASSET FILE ──────────────────────────────────────────────────────
//

enum class AssetFileFlags : u64 {
    None = 0,
};
consteval void enable_bitmask(AssetFileFlags);

struct TextureAssetFileHeader {
    vk::Extent3D extent = {};
    vk::Format format = vk::Format::Undefined;
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
    static auto create(Device_H) -> AssetManager;
    auto destroy() -> void;

    auto asset_root_path(AssetType type) -> fs::path;
    auto to_asset_file_type(const fs::path &path) -> AssetFileType;
    auto material_buffer() const -> Buffer &;
    auto registry() const -> const AssetRegistry &;

    // Add already existing asset into the registry.
    // File must end with `.lrasset` extension.
    auto add_asset(const fs::path &path) -> Asset *;

    //  ── Registered Assets ───────────────────────────────────────────────
    // These functions make engine aware of given asset, it does __NOT__
    // load them. Useful if we want to individually load scene/world assets.
    //
    auto register_asset(const fs::path &path, UUID &uuid, AssetType type) -> Asset *;
    auto register_model(const fs::path &path, UUID &uuid) -> Asset *;
    auto register_texture(const fs::path &path, UUID &uuid) -> Asset *;
    auto register_material(const fs::path &path, UUID &uuid) -> Asset *;
    auto register_scene(const fs::path &path, UUID &uuid) -> Asset *;

    //  ── Created Assets ──────────────────────────────────────────────────
    // Assets that will be created and asinged new UUID. New `.lrasset` file
    // will be written in the same directory as target asset.
    // All created assets will be automatically registered into the registry.
    //
    auto create_scene(const std::string &name, const fs::path &path) -> Asset *;
    auto write_asset_meta(Asset *asset, const UUID &uuid) -> void;

    //  ── Imported Assets ─────────────────────────────────────────────────
    // Assets that already exist in the filesystem with already set UUID's
    //

    //  ── Load Assets ─────────────────────────────────────────────────────
    // Load contents of registered assets.
    //
    auto load_model(Asset *asset) -> bool;
    auto load_texture(Asset *asset, vk::Format format, vk::Extent3D extent, ls::span<u8> pixels, Sampler sampler = {}) -> bool;
    auto load_texture(Asset *asset, Sampler sampler = {}) -> bool;
    auto load_material(Asset *asset, const Material &material_info) -> bool;
    auto load_material(Asset *asset) -> bool;
    auto load_scene(Asset *asset) -> bool;

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
    auto get_scene(const UUID &uuid) -> Scene;
    auto get_scene(SceneID scene_id) -> Scene;
};
}  // namespace lr
