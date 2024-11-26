#pragma once

#include "Identifier.hh"

#include "Engine/Graphics/Vulkan.hh"
#include "Engine/World/Model.hh"

namespace lr {
enum class AssetType : u32 {
    None = 0,
    Root = 0,
    Shader,
    Model,
    Texture,
    Material,
    Font,
};

enum class AssetFileType : u32 {
    None = 0,
    Binary,
    GLB,
    PNG,
    JPEG,
};

struct Asset {
    fs::path path = {};
    AssetType type = AssetType::None;
    union {
        ModelID model_id = ModelID::Invalid;
        TextureID texture_id;
        MaterialID material_id;
    };
};

struct AssetManager : Handle<AssetManager> {
    static auto create(Device_H) -> AssetManager;
    auto destroy() -> void;

    auto set_root_path(const fs::path &path) -> void;
    auto asset_root_path(AssetType type) -> fs::path;
    auto to_asset_file_type(const fs::path &path) -> AssetFileType;
    auto material_buffer() const -> Buffer &;
    auto gpu_models() const -> ls::span<GPUModel>;
    auto register_asset(const Identifier &ident, const fs::path &path, AssetType type) -> Asset *;

    auto load_texture(const Identifier &ident, vk::Format format, vk::Extent3D extent, ls::span<u8> pixels, Sampler sampler = {})
        -> TextureID;
    auto load_texture(const Identifier &ident, const fs::path &path, Sampler sampler = {}) -> TextureID;
    auto load_model(const Identifier &ident, const fs::path &path) -> ModelID;
    auto load_material(const Identifier &ident, const Material &material_info) -> MaterialID;
    auto load_material(const Identifier &ident, const fs::path &path) -> MaterialID;

    auto get_asset(const Identifier &ident) -> Asset *;
    auto get_model(const Identifier &ident) -> Model *;
    auto get_model(ModelID model_id) -> Model *;
    auto get_texture(const Identifier &ident) -> Texture *;
    auto get_texture(TextureID texture_id) -> Texture *;
    auto get_material(const Identifier &ident) -> Material *;
    auto get_material(MaterialID material_id) -> Material *;
};
}  // namespace lr
