#pragma once

#include "Identifier.hh"

#include "Engine/Graphics/Vulkan.hh"
#include "Engine/World/Model.hh"

namespace lr {
enum class AssetType : u32 {
    None = 0,
    Root = 0,
    Shader,
    Image,
    Model,
    Font,
};

enum class AssetFileType : u32 {
    None = 0,
    GLB,
    PNG,
    JPEG,
};

struct AssetManager : Handle<AssetManager> {
    static auto create(Device_H) -> AssetManager;
    auto destroy() -> void;

    auto set_root_path(const fs::path &path) -> void;
    auto asset_root_path(AssetType type) -> fs::path;

    auto add_texure(const Identifier &ident, ImageID image_id, SamplerID sampler_id) -> Texture *;

    auto load_model(const Identifier &ident, const fs::path &path) -> Model *;
    auto load_material(const Identifier &ident, const fs::path &path) -> Material *;

    auto model(const Identifier &ident) -> Model *;
    auto texture(const Identifier &ident) -> Texture *;
    auto material(const Identifier &ident) -> Material *;
};
}  // namespace lr
