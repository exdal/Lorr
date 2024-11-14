implementing "../lorr";

public struct Material {
    public vec4 albedo_color;
    public vec4 emissive_color;
    public f32 roughness_factor;
    public f32 metallic_factor;
    public u32 alpha_mode;
    public f32 alpha_cutoff;
    public SampledImage<float4> albedo_image;
    public SampledImage<float4> normal_image;
    public SampledImage<float4> emissive_image;
};

public struct Sun {
    public vec3 direction;
    public f32 intensity;
};

public struct Atmosphere {
    public vec3 rayleigh_scatter;
    public f32 rayleigh_density;

    public vec3 mie_scatter;
    public f32 mie_density;
    public f32 mie_extinction;
    public f32 mie_asymmetry;

    public vec3 ozone_absorption;
    public f32 ozone_height;
    public f32 ozone_thickness;

    public vec3 terrain_albedo;
    public f32 aerial_km_per_slice;
    public f32 planet_radius;
    public f32 atmos_radius;
};

public struct Clouds {
    public vec2 bounds;
    public f32 global_scale;
    public f32 shape_noise_scale;
    public vec3 shape_noise_weights;
    public f32 detail_noise_scale;
    public vec3 detail_noise_weights;
    public f32 detail_noise_influence;
    public f32 coverage;
    public f32 general_density;
    public vec3 phase_values;
    public f32 cloud_type;
    public f32 draw_distance;
    public f32 darkness_threshold;
};

public struct Camera {
    public mat4 projection_mat;
    public mat4 view_mat;
    public mat4 projection_view_mat;
    public mat4 inv_view_mat;
    public mat4 inv_projection_view_mat;
    public vec3 position;
    public float near_clip;
    public float far_clip;
};

public struct Model {
    public mat4 model_transform_mat;
    public mat4 world_transform_mat;
};

public struct World {
    public SamplerID linear_sampler;
    public SamplerID nearest_sampler;

    public Camera *cameras;
    public Material *materials;
    public Model *models;
    public u32 active_camera_index;

    public Sun sun;
    public Atmosphere atmosphere;
    public Clouds clouds;
};

