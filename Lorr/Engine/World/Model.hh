#pragma once

#include "Engine/Asset/Identifier.hh"
#include "Engine/Graphics/Vulkan.hh"

namespace lr {
struct Vertex {
    glm::vec3 position = {};
    f32 uv_x = 0.0f;
    glm::vec3 normal = {};
    f32 uv_y = 0.0f;
};

static Identifier INVALID_TEXTURE = "invalid_texture";
struct Texture {
    ImageID image_id = ImageID::Invalid;
    SamplerID sampler_id = SamplerID::Invalid;
};

static Identifier INVALID_MATERIAL = "invalid_material";
struct Material {
    glm::vec4 albedo_color = { 1.0f, 1.0f, 1.0f, 1.0f };
    glm::vec4 emissive_color = { 0.0f, 0.0f, 0.0f, 1.0f };
    f32 roughness_factor = 0.0f;
    f32 metallic_factor = 0.0f;
    vk::AlphaMode alpha_mode = vk::AlphaMode::Opaque;
    f32 alpha_cutoff = 0.0f;

    ls::option<Identifier> albedo_texture_ident = ls::nullopt;
    ls::option<Identifier> normal_texture_ident = ls::nullopt;
    ls::option<Identifier> emissive_texture_ident = ls::nullopt;
};

static Identifier INVALID_MODEL = "invalid_model";
struct Model {
    struct Primitive {
        u32 vertex_offset = 0;
        u32 vertex_count = 0;
        u32 index_offset = 0;
        u32 index_count = 0;
        Identifier material_ident = INVALID_MATERIAL;
    };

    struct Mesh {
        std::string name = {};
        std::vector<u32> primitive_indices = {};
        BufferID vertex_buffer_cpu = BufferID::Invalid;
        BufferID index_buffer_cpu = BufferID::Invalid;
        u32 total_vertex_count = 0;
        u32 total_index_count = 0;
    };

    std::vector<Primitive> primitives = {};
    std::vector<Mesh> meshes = {};
};

}  // namespace lr
