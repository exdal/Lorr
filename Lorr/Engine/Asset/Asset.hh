#pragma once

#include "Engine/Graphics/Common.hh"
#include "Engine/Graphics/Pipeline.hh"
#include "Engine/World/Model.hh"

namespace lr {
enum class AssetType : u32 {
    None = 0,
    Text,
    Image,
    Model,
};

// For future, we can have giant Vertex and Index buffers
// to ease out the Model management. But that might introduce
// more work, stuff like allocation, etc... Change it when needed.

struct AssetManager {
    constexpr static usize MAX_MATERIAL_COUNT = 64;

    Device *device = nullptr;

    // TODO: Maybe replace these with deque/colony
    std::vector<Texture> textures = {};
    std::vector<Material> materials = {};
    std::vector<Model> models = {};
    BufferID material_buffer_id = BufferID::Invalid;

    bool init(this AssetManager &, Device *device);

    ls::option<ModelID> load_model(this AssetManager &, std::string_view path);
    void refresh_materials(this AssetManager &);

    auto &model_at(ModelID model_id) { return models[static_cast<usize>(model_id)]; }
    auto &material_at(usize material_id) { return materials[material_id]; }

    constexpr static VertexAttribInfo VERTEX_LAYOUT[] = {
        { .format = Format::R32G32B32_SFLOAT, .location = 0, .offset = offsetof(Vertex, position) },
        { .format = Format::R32_SFLOAT, .location = 1, .offset = offsetof(Vertex, uv_x) },
        { .format = Format::R32G32B32_SFLOAT, .location = 2, .offset = offsetof(Vertex, normal) },
        { .format = Format::R32_SFLOAT, .location = 3, .offset = offsetof(Vertex, uv_y) },
        { .format = Format::R8G8B8A8_UNORM, .location = 4, .offset = offsetof(Vertex, color) },
    };
};
}  // namespace lr
