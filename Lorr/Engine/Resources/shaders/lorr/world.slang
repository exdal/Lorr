implementing lorr;

public struct Material {
    public vec4 albedo_color = { 1.0f, 1.0f, 1.0f, 1.0f };
    public vec4 emissive_color = { 0.0f, 0.0f, 0.0f, 1.0f };
    public f32 roughness_factor = 0.0f;
    public f32 metallic_factor = 0.0f;
    public u32 alpha_mode = 0u;
    public f32 alpha_cutoff = 0.0f;
    public ImageViewID albedo_image_view = {};
    public SamplerID albedo_sampler = {};
    public ImageViewID normal_image_view = {};
    public SamplerID normal_sampler = {};
    public ImageViewID emissive_image_view = {};
    public SamplerID emissive_sampler = {};
};

public struct Sun {
    public vec3 direction;
    public f32 intensity;
};

public struct Atmosphere {
    public vec3 rayleigh_scatter;
    public f32 rayleigh_density;

    public f32 planet_radius;
    public f32 atmos_radius;
    public f32 mie_scatter;
    public f32 mie_absorption;

    public f32 mie_density;
    public f32 mie_asymmetry;
    public f32 ozone_height;
    public f32 ozone_thickness;

    public vec3 ozone_absorption;
};

public struct Camera {
    public mat4 projection_mat;
    public mat4 view_mat;
    public mat4 projection_view_mat;
    public mat4 inv_view_mat;
    public mat4 inv_projection_view_mat;
    public vec3 position;
};

public struct Model {
    public mat4 model_transform_mat;
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
};

