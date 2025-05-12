#pragma once

#include "Engine/Asset/AssetFile.hh"

#include "Engine/Graphics/VulkanTypes.hh"

namespace lr {
enum class GLTFAlphaMode : u32 {
    Opaque = 0,
    Mask,
    Blend,
};

struct GLTFSamplerInfo {
    vuk::Filter mag_filter = {};
    vuk::Filter min_filter = {};
    vuk::SamplerAddressMode address_u = {};
    vuk::SamplerAddressMode address_v = {};
};

struct GLTFImageInfo {
    std::string name = {};
    AssetFileType file_type = {};
    std::variant<fs::path, std::vector<u8>> image_data = {};
};

struct GLTFTextureInfo {
    ls::option<usize> sampler_index;
    ls::option<usize> image_index;
    // TODO: dds images maybe?
};

struct GLTFMaterialInfo {
    glm::vec4 albedo_color = {};
    glm::vec4 emissive_color = {};
    f32 roughness_factor = 0.0f;
    f32 metallic_factor = 0.0f;
    GLTFAlphaMode alpha_mode = {};
    f32 alpha_cutoff = 0.0f;

    ls::option<u32> albedo_texture_index;
    ls::option<u32> normal_texture_index;
    ls::option<u32> emissive_texture_index;
    ls::option<u32> metallic_roughness_texture_index;
    ls::option<u32> occlusion_texture_index;
};

struct GLTFNodeInfo {
    std::string name = {};
    ls::option<usize> mesh_index = ls::nullopt;
    std::vector<usize> children = {};
    glm::vec3 translation = {};
    glm::quat rotation = {};
    glm::vec3 scale = {};
};

struct GLTFSceneInfo {
    std::string name = {};
    std::vector<usize> node_indices = {};
};

struct GLTFModelCallbacks {
    void *user_data = nullptr;
    // clang-format off
    void (*on_new_primitive)
       (void *user_data,
        u32 mesh_index,
        u32 material_index,
        u32 vertex_offset,
        u32 vertex_count,
        u32 index_offset,
        u32 index_count) = nullptr;
    // clang-format on

    // Accessors
    void (*on_access_index)(void *user_data, u32 mesh_index, u64 offset, u32 index) = nullptr;
    void (*on_access_position)(void *user_data, u32 mesh_index, u64 offset, glm::vec3 position) = nullptr;
    void (*on_access_normal)(void *user_data, u32 mesh_index, u64 offset, glm::vec3 normal) = nullptr;
    void (*on_access_texcoord)(void *user_data, u32 mesh_index, u64 offset, glm::vec2 texcoord) = nullptr;
    void (*on_access_color)(void *user_data, u32 mesh_index, u64 offset, glm::vec4 color) = nullptr;
};

struct GLTFModelInfo {
    std::vector<GLTFSamplerInfo> samplers = {};
    std::vector<GLTFImageInfo> images = {};
    std::vector<GLTFTextureInfo> textures = {};
    std::vector<GLTFMaterialInfo> materials = {};
    std::vector<GLTFNodeInfo> nodes = {};
    std::vector<GLTFSceneInfo> scenes = {};
    ls::option<usize> defualt_scene_index = ls::nullopt;

    static auto parse(const fs::path &path, GLTFModelCallbacks callbacks = {}) -> ls::option<GLTFModelInfo>;
    static auto parse_info(const fs::path &path) -> ls::option<GLTFModelInfo>;
};

} // namespace lr
