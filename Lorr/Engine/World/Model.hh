#pragma once

#include "Engine/Asset/Identifier.hh"
#include "Engine/Graphics/Vulkan.hh"

namespace lr {
struct Vertex {
    glm::vec3 position = {};
    f32 uv_x = 0.0f;
    glm::vec3 normal = {};
    f32 uv_y = 0.0f;
    u32 color = 0;
};

constexpr static vk::VertexAttribInfo VERTEX_LAYOUT[] = {
    { .format = vk::Format::R32G32B32_SFLOAT, .location = 0, .offset = offsetof(Vertex, position) },
    { .format = vk::Format::R32_SFLOAT, .location = 1, .offset = offsetof(Vertex, uv_x) },
    { .format = vk::Format::R32G32B32_SFLOAT, .location = 2, .offset = offsetof(Vertex, normal) },
    { .format = vk::Format::R32_SFLOAT, .location = 3, .offset = offsetof(Vertex, uv_y) },
    { .format = vk::Format::R8G8B8A8_UNORM, .location = 4, .offset = offsetof(Vertex, color) },
};

static Identifier INVALID_TEXTURE = "invalid_texture";
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

static Identifier INVALID_MATERIAL = "invalid_material";
struct Material {
    glm::vec4 albedo_color = { 1.0f, 1.0f, 1.0f, 1.0f };
    glm::vec4 emissive_color = { 0.0f, 0.0f, 0.0f, 1.0f };
    f32 roughness_factor = 0.0f;
    f32 metallic_factor = 0.0f;
    AlphaMode alpha_mode = AlphaMode::Opaque;
    f32 alpha_cutoff = 0.0f;

    ls::option<Identifier> albedo_texture = ls::nullopt;
    ls::option<Identifier> normal_texture = ls::nullopt;
    ls::option<Identifier> emissive_texture = ls::nullopt;
};

enum class GPUMaterialID : u32 { Invalid = ~0_u32 };
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

static Identifier INVALID_MODEL = "invalid_model";
struct Model {
    struct Primitive {
        usize vertex_offset = 0;
        usize vertex_count = 0;
        usize index_offset = 0;
        usize index_count = 0;
        Identifier material_ident = {};
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

enum class GPUModelID : u32 { Invalid = ~0_u32 };
struct GPUModel {
    glm::mat4 transform_mat = {};
};
}  // namespace lr
