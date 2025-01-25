implementing "../lorr";

public struct Sun {
    public vec3 direction;
    public f32 intensity;
};

public struct Atmosphere {
    public vec3 rayleigh_scatter;
    public f32  rayleigh_density;

    public vec3 mie_scatter;
    public f32  mie_density;
    public f32  mie_extinction;
    public f32  mie_asymmetry;

    public vec3 ozone_absorption;
    public f32  ozone_height;
    public f32  ozone_thickness;

    public vec3 terrain_albedo;
    public f32  aerial_km_per_slice;
    public f32  planet_radius;
    public f32  atmos_radius;
};

public struct Clouds {
    public vec2 bounds;
    public f32  global_scale;
    public f32  shape_noise_scale;
    public vec3 shape_noise_weights;
    public f32  detail_noise_scale;
    public vec3 detail_noise_weights;
    public f32  detail_noise_influence;
    public f32  coverage;
    public f32  general_density;
    public vec3 phase_values;
    public f32  cloud_type;
    public f32  draw_distance;
    public f32  darkness_threshold;
    public i32  sun_step_count;
    public i32  clouds_step_count;
    public f32  cloud_light_absorption;
    public f32  sun_light_absorption;
};

public struct Camera {
    public mat4 projection_mat;
    public mat4 view_mat;
    public mat4 projection_view_mat;
    public mat4 inv_view_mat;
    public mat4 inv_projection_view_mat;
    public vec3 position;
    public f32  near_clip;
    public f32  far_clip;
};

