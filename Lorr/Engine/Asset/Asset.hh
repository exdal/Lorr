#pragma once

#include "Identifier.hh"

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

using AssetRegistry = ankerl::unordered_dense::map<Identifier, Asset>;
struct AssetManager : Handle<AssetManager> {
    static auto create(Device_H) -> AssetManager;
    auto destroy() -> void;

    auto asset_root_path(AssetType type) -> fs::path;
    auto to_asset_file_type(const fs::path &path) -> AssetFileType;
    auto material_buffer() const -> Buffer &;
    auto registry() const -> const AssetRegistry &;

    //  ── Registered Assets ───────────────────────────────────────────────
    // These functions make engine aware of given asset, it does __NOT__
    // load them. Useful if we want to individually load scene/world assets.
    //
    auto register_asset(const Identifier &ident, const fs::path &path, AssetType type) -> Asset *;
    auto register_asset_from_file(const fs::path &path) -> Asset *;
    auto register_model(const Identifier &ident, const fs::path &path) -> Asset *;
    auto register_texture(const Identifier &ident, const fs::path &path) -> Asset *;
    auto register_material(const Identifier &ident, const fs::path &path) -> Asset *;
    auto register_scene(const Identifier &ident, const fs::path &path) -> Asset *;

    //  ── Created Assets ──────────────────────────────────────────────────
    // Assets that will be assigned new UUID's, must not exist in registry.
    //
    auto create_scene(Asset *asset, const std::string &name) -> Scene;

    //  ── Imported Assets ─────────────────────────────────────────────────
    // Assets that already exist in the filesystem with already set UUID's
    //
    auto load_model(const Identifier &ident, const fs::path &path) -> ModelID;
    auto load_texture(const Identifier &ident, vk::Format format, vk::Extent3D extent, ls::span<u8> pixels, Sampler sampler = {})
        -> TextureID;
    auto load_texture(const Identifier &ident, const fs::path &path, Sampler sampler = {}) -> TextureID;
    auto load_material(const Identifier &ident, const Material &material_info) -> MaterialID;
    auto load_material(const Identifier &ident, const fs::path &path) -> MaterialID;
    auto import_scene(Asset *asset) -> bool;

    auto export_scene(SceneID scene_id) -> bool;

    auto get_asset(const Identifier &ident) -> Asset *;
    auto get_model(const Identifier &ident) -> Model *;
    auto get_model(ModelID model_id) -> Model *;
    auto get_texture(const Identifier &ident) -> Texture *;
    auto get_texture(TextureID texture_id) -> Texture *;
    auto get_material(const Identifier &ident) -> Material *;
    auto get_material(MaterialID material_id) -> Material *;
    auto get_scene(const Identifier &ident) -> Scene;
    auto get_scene(SceneID scene_id) -> Scene;
};
}  // namespace lr
