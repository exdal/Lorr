#pragma once

#include "Engine/Asset/Asset.hh"

#include "Engine/Graphics/VulkanTypes.hh"

namespace lr {
struct GLTFVertex {
    glm::vec3 position = {};
    glm::vec3 normal = {};
    glm::vec2 tex_coord_0 = {};
    u32 color = 0;
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
    f32 alpha_cutoff = 0.0f;

    ls::option<u32> albedo_texture_index;
    ls::option<u32> normal_texture_index;
    ls::option<u32> emissive_texture_index;
};

struct GLTFPrimitiveInfo {
    u32 vertex_offset = 0;
    u32 vertex_count = 0;
    u32 index_offset = 0;
    u32 index_count = 0;
    u32 material_index = 0;
};

struct GLTFMeshInfo {
    std::string name = {};
    std::vector<u32> primitive_indices = {};
};

struct GLTFModelCallbacks {
    void *user_data = nullptr;
    void (*on_buffer_sizes)(void *user_data, u32 vertex_count, u32 index_count, u32 mesh_count) = nullptr;
    void (*on_mesh_finish)(void *user_data, u32 mesh_index, u32 vertex_count, u32 vertex_offset, u32 index_count, u32 index_offset) =
        nullptr;

    // Accessors
    void (*on_access_index)(void *user_data, u64 offset, u32 index) = nullptr;
    void (*on_access_position)(void *user_data, u64 offset, glm::vec3 position) = nullptr;
    void (*on_access_normal)(void *user_data, u64 offset, glm::vec3 normal) = nullptr;
    void (*on_access_texcoord)(void *user_data, u64 offset, glm::vec2 texcoord) = nullptr;
    void (*on_access_color)(void *user_data, u64 offset, glm::vec4 color) = nullptr;
};

struct GLTFModelInfo {
    std::vector<GLTFSamplerInfo> samplers = {};
    std::vector<GLTFImageInfo> images = {};
    std::vector<GLTFTextureInfo> textures = {};
    std::vector<GLTFMaterialInfo> materials = {};
    std::vector<GLTFPrimitiveInfo> primitives = {};
    std::vector<GLTFMeshInfo> meshes = {};

    static auto parse(const fs::path &path, GLTFModelCallbacks callbacks = {}) -> ls::option<GLTFModelInfo>;
};

}  // namespace lr
