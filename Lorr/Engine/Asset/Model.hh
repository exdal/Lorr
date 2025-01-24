#pragma once

#include "Engine/Asset/UUID.hh"

#include "Engine/Graphics/Vulkan.hh"

namespace lr {
struct TextureSamplerInfo {
    vuk::Filter mag_filter = vuk::Filter::eLinear;
    vuk::Filter min_filter = vuk::Filter::eLinear;
    vuk::SamplerAddressMode address_u = vuk::SamplerAddressMode::eRepeat;
    vuk::SamplerAddressMode address_v = vuk::SamplerAddressMode::eRepeat;
};

enum class TextureID : u64 { Invalid = std::numeric_limits<u64>::max() };
struct Texture {
    Image image = {};
    ImageView image_view = {};
    Sampler sampler = {};

    SampledImage sampled_image() { return { image_view.id(), sampler.id() }; }
};

enum class MaterialID : u64 { Invalid = std::numeric_limits<u64>::max() };
struct Material {
    glm::vec3 albedo_color = { 1.0f, 1.0f, 1.0f };
    glm::vec3 emissive_color = { 0.0f, 0.0f, 0.0f };
    f32 roughness_factor = 0.0f;
    f32 metallic_factor = 0.0f;
    f32 alpha_cutoff = 0.0f;
    UUID albedo_texture = {};
    UUID normal_texture = {};
    UUID emissive_texture = {};
};

enum class ModelID : u64 { Invalid = std::numeric_limits<u64>::max() };
struct Model {
    struct Meshlet {
        u32 vertex_offset = 0;
        u32 index_count = 0;
        u32 index_offset = 0;
        u32 triangle_count = 0;
        u32 triangle_offset = 0;
        MaterialID material_id = MaterialID::Invalid;
    };

    struct Mesh {
        std::string name = {};
        std::vector<u32> meshlet_indices = {};
    };

    struct Node {
        std::vector<u32> mesh_indices = {};
        std::vector<u32> child_indices = {};
        glm::mat4 transform = {};
        std::string name = {};
    };

    std::vector<UUID> textures = {};
    std::vector<UUID> materials = {};
    std::vector<Meshlet> meshlets = {};
    std::vector<Mesh> meshes = {};
    std::vector<Node> nodes = {};

    Buffer vertex_buffer = {};
    Buffer index_buffer = {};
};
}  // namespace lr
