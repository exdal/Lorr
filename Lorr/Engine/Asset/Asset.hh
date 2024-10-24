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

struct AssetManager {
    Device device = {};
    ankerl::unordered_dense::map<Identifier, Texture> textures = {};
    ankerl::unordered_dense::map<Identifier, Material> materials = {};
    ankerl::unordered_dense::map<Identifier, Model> models = {};

    bool init(this AssetManager &, Device device);
    void shutdown(this AssetManager &, bool print_reports);

    Model *load_model(this AssetManager &, const Identifier &ident, const fs::path &path);
    Material *add_material(this AssetManager &, const Identifier &ident, const Material &material_data);

    fs::path asset_dir(AssetType type);

    Texture *texture_at(this AssetManager &, const Identifier &ident);
    Material *material_at(this AssetManager &, const Identifier &ident);
    Model *model_at(this AssetManager &, const Identifier &ident);
};
}  // namespace lr
