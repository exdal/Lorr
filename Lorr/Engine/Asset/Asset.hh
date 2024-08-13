#pragma once

#include "Engine/Graphics/Common.hh"
#include "Engine/Graphics/Pipeline.hh"
#include "Engine/World/Model.hh"

namespace lr {
enum class AssetType : u32 {
    None = 0,
    Shader,
    Image,
    Model,
};

// For future, we can have giant Vertex and Index buffers
// to ease out the Model management. But that might introduce
// more work, stuff like allocation, etc... Change it when needed.

struct AssetManager {
    constexpr static usize MAX_MATERIAL_COUNT = 64;

    Device *device = nullptr;
    BufferID material_buffer_id = BufferID::Invalid;

    // TODO: Maybe replace these with deque/colony
    std::vector<Texture> textures = {};
    std::vector<Material> materials = {};
    std::vector<Model> models = {};
    std::vector<Shader> shaders = {};

    bool init(this AssetManager &, Device *device);

    ls::option<ModelID> load_model(this AssetManager &, const fs::path &path);
    ls::option<MaterialID> add_material(this AssetManager &, const Material &material);
    ls::option<ShaderID> load_shader(this AssetManager &, const fs::path &path);

    auto &model_at(ModelID model_id) { return models[static_cast<usize>(model_id)]; }
    auto &material_at(ModelID material_id) { return materials[static_cast<usize>(material_id)]; }

    constexpr static VertexAttribInfo VERTEX_LAYOUT[] = {
        { .format = Format::R32G32B32_SFLOAT, .location = 0, .offset = offsetof(Vertex, position) },
        { .format = Format::R32_SFLOAT, .location = 1, .offset = offsetof(Vertex, uv_x) },
        { .format = Format::R32G32B32_SFLOAT, .location = 2, .offset = offsetof(Vertex, normal) },
        { .format = Format::R32_SFLOAT, .location = 3, .offset = offsetof(Vertex, uv_y) },
        { .format = Format::R8G8B8A8_UNORM, .location = 4, .offset = offsetof(Vertex, color) },
    };
};
}  // namespace lr
