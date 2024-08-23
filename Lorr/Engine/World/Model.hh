#pragma once

#include "Engine/Graphics/zfwd.hh"

namespace lr {
struct Vertex {
    glm::vec3 position = {};
    f32 uv_x = 0.0f;
    glm::vec3 normal = {};
    f32 uv_y = 0.0f;
    u32 color = 0;
};

struct Texture {
    ImageID image_id = ImageID::Invalid;
    ImageViewID image_view_id = ImageViewID::Invalid;
    SamplerID sampler_id = SamplerID::Invalid;
};

enum class AlphaMode : u32 {
    Opaque = 0,
    Mask,
    Blend,
};

enum class MaterialID : u32 { Invalid = ~0_u32 };
struct Material {
    glm::vec4 albedo_color = { 1.0f, 1.0f, 1.0f, 1.0f };
    glm::vec4 emissive_color = { 0.0f, 0.0f, 0.0f, 1.0f };
    f32 roughness_factor = 0.0f;
    f32 metallic_factor = 0.0f;
    AlphaMode alpha_mode = AlphaMode::Opaque;
    f32 alpha_cutoff = 0.0f;

    ls::option<usize> albedo_texture_index;
    ls::option<usize> normal_texture_index;
    ls::option<usize> emissive_texture_index;
};

struct GPUMaterial {
    glm::vec4 albedo_color = { 1.0f, 1.0f, 1.0f, 1.0f };
    glm::vec4 emissive_color = { 0.0f, 0.0f, 0.0f, 1.0f };
    f32 roughness_factor = 0.0f;
    f32 metallic_factor = 0.0f;
    AlphaMode alpha_mode = AlphaMode::Opaque;
    f32 alpha_cutoff = 0.0f;
    ImageViewID albedo_image_view = ImageViewID::Invalid;
    SamplerID albedo_sampler = SamplerID::Invalid;
    ImageViewID normal_image_view = ImageViewID::Invalid;
    SamplerID normal_sampler = SamplerID::Invalid;
    ImageViewID emissive_image_view = ImageViewID::Invalid;
    SamplerID emissive_sampler = SamplerID::Invalid;
};

enum class ModelID : usize { Invalid = ~0_sz };
struct Model {
    struct Primitive {
        usize vertex_offset = 0;
        usize vertex_count = 0;
        usize index_offset = 0;
        usize index_count = 0;
        usize material_index = 0;
    };

    struct Mesh {
        std::string name = {};
        std::vector<usize> primitive_indices = {};
    };

    std::vector<Primitive> primitives = {};
    std::vector<Mesh> meshes = {};

    ls::option<BufferID> vertex_buffer;
    ls::option<BufferID> index_buffer;
};
}  // namespace lr
