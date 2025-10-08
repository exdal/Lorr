#pragma once

#include "Engine/Asset/UUID.hh"
#include "Engine/Graphics/Vulkan.hh"

namespace lr {
struct TextureInfo {
    bool use_srgb = true;
};

enum class TextureID : u64 { Invalid = std::numeric_limits<u64>::max() };
struct Texture {
    Image image = {};
    ImageView image_view = {};
};

enum class AlphaMode : u32 {
    Opaque = 0,
    Mask,
    Blend,
};

enum class MaterialID : u64 { Invalid = std::numeric_limits<u64>::max() };
struct Material {
    glm::vec4 albedo_color = { 1.0f, 1.0f, 1.0f, 1.0f };
    glm::vec4 emissive_color = { 0.0f, 0.0f, 0.0f, 1.0f };
    f32 roughness_factor = 0.0f;
    f32 metallic_factor = 0.0f;
    AlphaMode alpha_mode = AlphaMode::Opaque;
    f32 alpha_cutoff = 0.0f;
    UUID albedo_texture = {};
    Sampler albedo_sampler = {};
    UUID normal_texture = {};
    Sampler normal_sampler = {};
    UUID emissive_texture = {};
    Sampler emissive_sampler = {};
    UUID metallic_roughness_texture = {};
    Sampler metallic_roughness_sampler = {};
    UUID occlusion_texture = {};
    Sampler occlusion_sampler = {};
};

struct MaterialInfo {
    glm::vec4 albedo_color = { 1.0f, 1.0f, 1.0f, 1.0f };
    glm::vec4 emissive_color = { 0.0f, 0.0f, 0.0f, 1.0f };
    f32 roughness_factor = 0.0f;
    f32 metallic_factor = 0.0f;
    AlphaMode alpha_mode = AlphaMode::Opaque;
    f32 alpha_cutoff = 0.0f;
    UUID albedo_texture = {};
    SamplerInfo albedo_sampler_info = {};
    UUID normal_texture = {};
    SamplerInfo normal_sampler_info = {};
    UUID emissive_texture = {};
    SamplerInfo emissive_sampler_info = {};
    UUID metallic_roughness_texture = {};
    SamplerInfo metallic_roughness_sampler_info = {};
    UUID occlusion_texture = {};
    SamplerInfo occlusion_sampler_info = {};
};

} // namespace lr
