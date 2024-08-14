#pragma once

#include "Engine/Graphics/Common.hh"
#include "Engine/World/Model.hh"

#include <fastgltf/types.hpp>

namespace lr {
enum class AssetFileType : u32 {
    None = 0,
    GLB,
    PNG,
    JPEG,
};

struct ImageAssetData {
    u32 width = 0;
    u32 height = 0;
    Format format = {};

    u8 *data = nullptr;
    usize data_size = 0;

    void (*deleter)(u8 *) = nullptr;

    ~ImageAssetData() {
        if (deleter) {
            deleter(data);
        }
    }
};

struct ModelAssetData {
    struct SamplerData {
        Filtering mag_filter = {};
        Filtering min_filter = {};
        TextureAddressMode address_u = {};
        TextureAddressMode address_v = {};
    };

    struct TextureData {
        std::string_view name = {};
        AssetFileType file_type = {};
        std::vector<u8> data = {};
        ls::option<usize> sampler_index;
    };

    struct MaterialData {
        glm::vec4 albedo_color = {};
        glm::vec4 emissive_color = {};
        f32 roughness_factor = 0.0f;
        f32 metallic_factor = 0.0f;
        AlphaMode alpha_mode = {};
        f32 alpha_cutoff = 0.0f;

        ls::option<usize> albedo_image_data_index;
        ls::option<usize> normal_image_data_index;
        ls::option<usize> emissive_image_data_index;
    };

    struct MeshPrimitiveData {
        ls::option<usize> vertex_offset;
        ls::option<usize> vertex_count;
        ls::option<usize> index_offset;
        ls::option<usize> index_count;
        ls::option<usize> material_index;
    };

    struct MeshAssetData {
        std::string name = {};
        std::vector<usize> primitive_indices = {};
    };

    std::vector<MeshPrimitiveData> primitives = {};
    std::vector<MeshAssetData> meshes = {};
    std::vector<MaterialData> materials = {};
    std::vector<Vertex> vertices = {};
    std::vector<u32> indices = {};

    std::vector<SamplerData> samplers = {};
    std::vector<TextureData> textures = {};
};

std::unique_ptr<ImageAssetData> parse_image_stbi(u8 *data, usize data_size);
std::unique_ptr<ModelAssetData> parse_model_gltf(u8 *data, usize data_size);

}  // namespace lr
