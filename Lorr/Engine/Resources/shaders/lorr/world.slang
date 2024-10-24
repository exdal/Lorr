implementing lorr;

struct Material {
    float4 albedo_color = { 1.0f, 1.0f, 1.0f, 1.0f };
    float4 emissive_color = { 0.0f, 0.0f, 0.0f, 1.0f };
    f32 roughness_factor = 0.0f;
    f32 metallic_factor = 0.0f;
    u32 alpha_mode = 0u;
    f32 alpha_cutoff = 0.0f;
    ImageViewID albedo_image_view = INVALID;
    SamplerID albedo_sampler = INVALID;
    ImageViewID normal_image_view = INVALID;
    SamplerID normal_sampler = INVALID;
    ImageViewID emissive_image_view = INVALID;
    SamplerID emissive_sampler = INVALID;
};

struct Sun {
    float3 direction;
    f32 intensity;
};

struct Atmosphere {
    float3 rayleigh_scatter;
    float rayleigh_density;

    float planet_radius;
    float atmos_radius;
    float mie_scatter;
    float mie_absorption;

    float mie_density;
    float mie_asymmetry;
    float ozone_height;
    float ozone_thickness;

    float3 ozone_absorption;
};

struct Camera {
    float4x4 projection_mat;
    float4x4 view_mat;
    float4x4 projection_view_mat;
    float4x4 inv_view_mat;
    float4x4 inv_projection_view_mat;
    float3 position;
};

struct Model {
    float4x4 model_transform_mat;
};

struct World {
    Camera *cameras;
    Material *materials;
    Model *models;
    Sun sun;
    Atmosphere atmosphere;
};

