#pragma once

#include "Engine/Asset/AssetFile.hh"
#include "Engine/Asset/UUID.hh"

#include "Engine/Graphics/Vulkan.hh"

namespace lr {
struct TextureSamplerInfo {
    vuk::Filter mag_filter = vuk::Filter::eLinear;
    vuk::Filter min_filter = vuk::Filter::eLinear;
    vuk::SamplerAddressMode address_u = vuk::SamplerAddressMode::eRepeat;
    vuk::SamplerAddressMode address_v = vuk::SamplerAddressMode::eRepeat;
};

struct TextureInfo {
    bool use_srgb = true;

    std::vector<u8> embedded_data = {}; // Optional
    AssetFileType file_type = AssetFileType::None; // Optional
};

enum class TextureID : u64 { Invalid = std::numeric_limits<u64>::max() };
struct Texture {
    Image image = {};
    ImageView image_view = {};
    Sampler sampler = {};
};

enum class AlphaMode : u32 {
    Opaque = 0,
    Mask,
    Blend,
};

enum class MaterialID : u64 { Invalid = std::numeric_limits<u64>::max() };
struct Material {
    glm::vec4 albedo_color = { 1.0f, 1.0f, 1.0f, 1.0f };
    glm::vec3 emissive_color = { 0.0f, 0.0f, 0.0f };
    f32 roughness_factor = 0.0f;
    f32 metallic_factor = 0.0f;
    AlphaMode alpha_mode = AlphaMode::Opaque;
    f32 alpha_cutoff = 0.0f;
    UUID albedo_texture = {};
    UUID normal_texture = {};
    UUID emissive_texture = {};
    UUID metallic_roughness_texture = {};
    UUID occlusion_texture = {};
};

struct MaterialInfo {
    Material material = {};
    TextureInfo albedo_texture_info = {};
    TextureInfo normal_texture_info = {};
    TextureInfo emissive_texture_info = {};
    TextureInfo metallic_roughness_texture_info = {};
    TextureInfo occlusion_texture_info = {};
};

enum class ModelID : u64 { Invalid = std::numeric_limits<u64>::max() };
struct Model {
    constexpr static auto MAX_MESHLET_INDICES = 64_sz;
    constexpr static auto MAX_MESHLET_PRIMITIVES = 64_sz;

    using Index = u32;

    struct Primitive {
        u32 material_index = 0;
        u32 meshlet_count = 0;
        u32 meshlet_offset = 0;
        u32 local_triangle_indices_offset = 0;
        u32 vertex_count = 0;
        u32 vertex_offset = 0;
        u32 index_count = 0;
        u32 index_offset = 0;
    };

    struct Mesh {
        std::string name = {};
        std::vector<u32> primitive_indices = {};
    };

    struct Node {
        std::string name = {};
        std::vector<usize> child_indices = {};
        ls::option<usize> mesh_index = ls::nullopt;
        glm::vec3 translation = {};
        glm::quat rotation = {};
        glm::vec3 scale = {};
    };

    struct Scene {
        std::string name = {};
        std::vector<usize> node_indices = {};
    };

    std::vector<UUID> embedded_textures = {};
    std::vector<UUID> materials = {};
    std::vector<Primitive> primitives = {};
    std::vector<Mesh> meshes = {};
    std::vector<Node> nodes = {};
    std::vector<Scene> scenes = {};

    usize default_scene_index = 0;

    Buffer indices = {};
    Buffer vertex_positions = {};
    Buffer vertex_normals = {};
    Buffer texture_coords = {};
    Buffer meshlets = {};
    Buffer meshlet_bounds = {};
    Buffer local_triangle_indices = {};
};
} // namespace lr
