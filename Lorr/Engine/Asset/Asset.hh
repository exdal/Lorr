#pragma once

#include "Identifier.hh"

#include "Engine/Graphics/Slang/Compiler.hh"
#include "Engine/Graphics/Vulkan.hh"
#include "Engine/World/Model.hh"

namespace lr {
enum class AssetType : u32 {
    None = 0,
    Shader,
    Image,
    Model,
};

struct AssetManager {
    Device device = {};
    SlangCompiler shader_compiler = {};
    ankerl::unordered_dense::map<Identifier, Texture> textures = {};
    ankerl::unordered_dense::map<Identifier, Material> materials = {};
    ankerl::unordered_dense::map<Identifier, Model> models = {};

    bool init(this AssetManager &, Device device);
    void shutdown(this AssetManager &, bool print_reports);

    ls::option<SlangModule> load_shader(this AssetManager &, const ShaderSessionInfo &session_info, const ShaderCompileInfo &compile_info);
    Model *load_model(this AssetManager &, const Identifier &ident, const fs::path &path);
    Material *add_material(this AssetManager &, const Identifier &ident, const Material &material_data);

    Texture *texture_at(this AssetManager &, const Identifier &ident);
    Material *material_at(this AssetManager &, const Identifier &ident);
    Model *model_at(this AssetManager &, const Identifier &ident);
};
}  // namespace lr
