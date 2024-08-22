#pragma once

#include "Identifier.hh"

#include "Engine/Graphics/Common.hh"
#include "Engine/Graphics/Pipeline.hh"
#include "Engine/Util/VirtualDir.hh"
#include "Engine/World/Model.hh"

namespace lr {
enum class AssetType : u32 {
    None = 0,
    Shader,
    Image,
    Model,
};

enum class ShaderCompileFlag : u32 {
    None = 0,
    GenerateDebugInfo = 1 << 1,
    SkipOptimization = 1 << 2,
    SkipValidation = 1 << 3,
    MatrixRowMajor = 1 << 4,
    MatrixColumnMajor = 1 << 5,
    InvertY = 1 << 6,
    DXPositionW = 1 << 7,
    UseGLLayout = 1 << 8,
    UseScalarLayout = 1 << 9,
};
template<>
struct has_bitmask<lr::ShaderCompileFlag> : std::true_type {};

struct ShaderPreprocessorMacroInfo {
    std::string_view name = {};
    std::string_view value = {};
};

struct ShaderCompileInfo {
    ls::span<ShaderPreprocessorMacroInfo> definitions = {};
    ShaderCompileFlag compile_flags = ShaderCompileFlag::MatrixRowMajor;
    std::string_view entry_point = "main";
    fs::path path = {};
    ls::option<std::string_view> source = ls::nullopt;
};

// For future, we can have giant Vertex and Index buffers
// to ease out the Model management. But that might introduce
// more work, stuff like allocation, etc... Change it when needed.

struct AssetManager {
    constexpr static usize MAX_MATERIAL_COUNT = 64;

    Device *device = nullptr;
    BufferID material_buffer_id = BufferID::Invalid;

    std::vector<Texture> textures = {};
    std::vector<Material> materials = {};
    std::vector<Model> models = {};

    VirtualDir shader_virtual_env = {};
    ankerl::unordered_dense::map<Identifier, ShaderID> shaders = {};

    bool init(this AssetManager &, Device *device);

    ls::option<ModelID> load_model(this AssetManager &, const fs::path &path);
    ls::option<MaterialID> add_material(this AssetManager &, const Material &material);
    // If `ShaderCompileInfo::source` has value, shader is loaded from memory.
    // But `path` is STILL used to print debug information.
    ls::option<ShaderID> load_shader(this AssetManager &, Identifier ident, const ShaderCompileInfo &info);

    ls::option<ShaderID> shader_at(this AssetManager &, Identifier ident);

    auto &model_at(ModelID model_id) { return models[static_cast<usize>(model_id)]; }
    auto &material_at(MaterialID material_id) { return materials[static_cast<usize>(material_id)]; }

    constexpr static VertexAttribInfo VERTEX_LAYOUT[] = {
        { .format = Format::R32G32B32_SFLOAT, .location = 0, .offset = offsetof(Vertex, position) },
        { .format = Format::R32_SFLOAT, .location = 1, .offset = offsetof(Vertex, uv_x) },
        { .format = Format::R32G32B32_SFLOAT, .location = 2, .offset = offsetof(Vertex, normal) },
        { .format = Format::R32_SFLOAT, .location = 3, .offset = offsetof(Vertex, uv_y) },
        { .format = Format::R8G8B8A8_UNORM, .location = 4, .offset = offsetof(Vertex, color) },
    };
};
}  // namespace lr
