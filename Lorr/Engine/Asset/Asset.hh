#pragma once

#include "Identifier.hh"

#include "Engine/Graphics/Common.hh"
#include "Engine/Graphics/Pipeline.hh"
#include "Engine/Graphics/Shader.hh"
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

struct ShaderProgram {
    struct GraphicsProgram {
        ShaderID vertex_shader;
        ShaderID pixel_shader;
    };

    struct ComputeProgram {
        glm::u64vec3 thread_group_size = {};
        ShaderID compute_shader;
    };

    PipelineLayoutID pipeline_layout_id = PipelineLayoutID::None;
    std::variant<std::monostate, GraphicsProgram, ComputeProgram> program = {};
};

struct AssetManager {
    Device *device = nullptr;

    VirtualDir shader_virtual_env = {};
    ankerl::unordered_dense::map<Identifier, PipelineID> pipelines = {};
    ankerl::unordered_dense::map<Identifier, ShaderID> shaders = {};
    ankerl::unordered_dense::map<Identifier, Texture> textures = {};
    ankerl::unordered_dense::map<Identifier, Material> materials = {};
    ankerl::unordered_dense::map<Identifier, Model> models = {};

    bool init(this AssetManager &, Device *device);
    void shutdown(this AssetManager &, bool print_reports);

    // If `ShaderCompileInfo::source` has value, shader is loaded from memory.
    // But `path` is STILL used to print debug information.
    ls::option<ShaderProgram> load_shader_program(this AssetManager &, const Identifier &ident, const ShaderCompileInfo &info);
    Model *load_model(this AssetManager &, const Identifier &ident, const fs::path &path);
    Material *add_material(this AssetManager &, const Identifier &ident, const Material &material_data);

    ls::option<ShaderID> shader_at(this AssetManager &, const Identifier &ident);
    Texture *texture_at(this AssetManager &, const Identifier &ident);
    Material *material_at(this AssetManager &, const Identifier &ident);
    Model *model_at(this AssetManager &, const Identifier &ident);

    constexpr static VertexAttribInfo VERTEX_LAYOUT[] = {
        { .format = Format::R32G32B32_SFLOAT, .location = 0, .offset = offsetof(Vertex, position) },
        { .format = Format::R32_SFLOAT, .location = 1, .offset = offsetof(Vertex, uv_x) },
        { .format = Format::R32G32B32_SFLOAT, .location = 2, .offset = offsetof(Vertex, normal) },
        { .format = Format::R32_SFLOAT, .location = 3, .offset = offsetof(Vertex, uv_y) },
        { .format = Format::R8G8B8A8_UNORM, .location = 4, .offset = offsetof(Vertex, color) },
    };
};
}  // namespace lr
