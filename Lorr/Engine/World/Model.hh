#pragma once

#include "Engine/Graphics/Vulkan.hh"

namespace lr {
enum class TextureID : u32 { Invalid = std::numeric_limits<u32>::max() };
struct Texture {
    Image image = {};
    Sampler sampler = {};

    SampledImage sampled_image() { return { image.view().id(), sampler.id() }; }
};

enum class MaterialID : u32 { Invalid = std::numeric_limits<u32>::max() };
struct Material {
    glm::vec4 albedo_color = { 1.0f, 1.0f, 1.0f, 1.0f };
    glm::vec4 emissive_color = { 0.0f, 0.0f, 0.0f, 1.0f };
    f32 roughness_factor = 0.0f;
    f32 metallic_factor = 0.0f;
    vk::AlphaMode alpha_mode = vk::AlphaMode::Opaque;
    f32 alpha_cutoff = 0.0f;
    TextureID albedo_texture_id = TextureID::Invalid;
    TextureID normal_texture_id = TextureID::Invalid;
    TextureID emissive_texture_id = TextureID::Invalid;
};

struct GPUMaterial {
    alignas(4) glm::vec4 albedo_color = { 1.0f, 1.0f, 1.0f, 1.0f };
    alignas(4) glm::vec4 emissive_color = { 0.0f, 0.0f, 0.0f, 1.0f };
    alignas(4) f32 roughness_factor = 0.0f;
    alignas(4) f32 metallic_factor = 0.0f;
    alignas(4) vk::AlphaMode alpha_mode = vk::AlphaMode::Opaque;
    alignas(4) f32 alpha_cutoff = 0.0f;
    alignas(4) SampledImage albedo_image = {};
    alignas(4) SampledImage normal_image = {};
    alignas(4) SampledImage emissive_image = {};
};

enum class ModelID : u32 { Invalid = std::numeric_limits<u32>::max() };
struct Model {
    struct Vertex {
        alignas(4) glm::vec3 position = {};
        alignas(4) f32 uv_x = 0.0f;
        alignas(4) glm::vec3 normal = {};
        alignas(4) f32 uv_y = 0.0f;
    };

    using Index = u32;

    struct Primitive {
        u32 vertex_offset = 0;
        u32 vertex_count = 0;
        u32 index_offset = 0;
        u32 index_count = 0;
        MaterialID material_id = MaterialID::Invalid;
    };

    struct Mesh {
        std::string name = {};
        std::vector<u32> primitive_indices = {};
    };

    std::vector<Primitive> primitives = {};
    std::vector<Mesh> meshes = {};
    Buffer vertex_buffer = {};
    Buffer index_buffer = {};
};

struct GPUModel {
    struct Primitive {
        u32 vertex_offset = 0;
        u32 vertex_count = 0;
        u32 index_offset = 0;
        u32 index_count = 0;
        u32 material_index = 0;
    };

    struct Mesh {
        std::vector<u32> primitive_indices = {};
    };

    std::vector<Primitive> primitives = {};
    std::vector<Mesh> meshes = {};

    BufferID vertex_bufffer_id = BufferID::Invalid;
    BufferID index_buffer_id = BufferID::Invalid;
};

}  // namespace lr
