#pragma once

#include "Engine/Graphics/Vulkan.hh"

namespace lr {
struct GLTFSamplerInfo {
    vk::Filtering mag_filter = {};
    vk::Filtering min_filter = {};
    vk::SamplerAddressMode address_u = {};
    vk::SamplerAddressMode address_v = {};
};

struct GLTFImageInfo {
    std::string name = {};
    vk::Format format = vk::Format::Undefined;
    vk::Extent2D extent = {};
    ls::option<BufferID> buffer_id = ls::nullopt;
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
    vk::AlphaMode alpha_mode = {};
    f32 alpha_cutoff = 0.0f;

    ls::option<u32> albedo_texture_index;
    ls::option<u32> normal_texture_index;
    ls::option<u32> emissive_texture_index;
};

struct GLTFPrimitiveInfo {
    ls::option<u32> vertex_offset;
    ls::option<u32> vertex_count;
    ls::option<u32> index_offset;
    ls::option<u32> index_count;
    ls::option<u32> material_index;
};

struct GLTFMeshInfo {
    std::string name = {};
    std::vector<u32> primitive_indices = {};
    ls::option<BufferID> vertex_buffer_cpu = ls::nullopt;
    ls::option<BufferID> index_buffer_cpu = ls::nullopt;
    u32 total_vertex_count = 0;
    u32 total_index_count = 0;
};

struct GLTFModelInfo {
    std::vector<GLTFSamplerInfo> samplers = {};
    std::vector<GLTFImageInfo> images = {};
    std::vector<GLTFTextureInfo> textures = {};
    std::vector<GLTFMaterialInfo> materials = {};
    std::vector<GLTFPrimitiveInfo> primitives = {};
    std::vector<GLTFMeshInfo> meshes = {};

    static auto parse(Device device, const fs::path &path) -> ls::option<GLTFModelInfo>;
    auto destroy(Device device) -> void;
};

}  // namespace lr
