#pragma once

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
    vuk::Format format = vuk::Format::eUndefined;
    vuk::Extent3D extent = {};
    std::vector<u8> pixels = {};
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

struct GLTFModelInfo {
    std::vector<GLTFSamplerInfo> samplers = {};
    std::vector<GLTFImageInfo> images = {};
    std::vector<GLTFTextureInfo> textures = {};
    std::vector<GLTFMaterialInfo> materials = {};
    std::vector<GLTFPrimitiveInfo> primitives = {};
    std::vector<GLTFMeshInfo> meshes = {};
    std::vector<GLTFVertex> vertices = {};
    std::vector<u32> indices = {};

    static auto parse(const fs::path &path) -> ls::option<GLTFModelInfo>;
};

}  // namespace lr
