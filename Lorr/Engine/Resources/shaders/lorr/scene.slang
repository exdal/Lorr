implementing "../lorr";

[Flags]
public enum CullFlags : u32 {
    MeshletFrustum,
    TriangleBackFace,
    MicroTriangles,
};

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
    public f32  aerial_gain_per_slice;
    public f32  planet_radius;
    public f32  atmos_radius;

    public ivec3 transmittance_lut_size;
    public ivec3 sky_view_lut_size;
    public ivec3 multiscattering_lut_size;
    public ivec3 aerial_perspective_lut_size;
};

public struct Clouds {
    public vec2 bounds;
    public vec4 shape_noise_weights;
    public vec4 detail_noise_weights;
    public f32  shape_noise_scale;
    public f32  detail_noise_scale;
    public f32  detail_noise_influence;
    public f32  coverage;
    public f32  general_density;
    public vec3 phase_values;
    public f32  cloud_type;
    public f32  draw_distance;
    public f32  darkness_threshold;
    public i32  sun_step_count;
    public i32  clouds_step_count;
    public f32  extinction_coeff;
    public f32  scattering_coeff;
    public f32  powder_intensity;

    public ivec3 noise_lut_size;
};

public struct Camera {
    public mat4 projection_mat;
    public mat4 view_mat;
    public mat4 projection_view_mat;
    public mat4 inv_view_mat;
    public mat4 inv_projection_view_mat;
    public mat4 frustum_projection_view_mat;
    public vec3 position;
    public f32  near_clip;
    public f32  far_clip;
    public vec4 frustum_planes[6];
    public vec2 resolution;
};

public struct Scene {
    public Camera camera;
    public Sun sun;
    public Atmosphere atmosphere;
    public Clouds clouds;

    public u32 meshlet_instance_count;
    public CullFlags cull_flags;
};
