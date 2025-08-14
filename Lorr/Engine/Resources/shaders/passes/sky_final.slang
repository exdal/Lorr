module sky_view;

import std;
import gpu;
import sky;
import scene;

struct ShaderParameters {
    Sampler sampler;
    Image2D<f32x4> sky_transmittance_lut;
    Image3D<f32x4> sky_aerial_perspective_lut;
    Image2D<f32x4> sky_view_lut;
    Image2D<f32> depth_image;
    ConstantBuffer<Environment> environment;
    ConstantBuffer<Camera> camera;
};

struct VertexOutput {
    f32x4 position : SV_Position;
    f32x2 tex_coord : TEXCOORD;
};

[[shader("vertex")]]
func vs_main(u32 vertex_index : SV_VertexID) -> VertexOutput {
    VertexOutput output;
    output.tex_coord = f32x2((vertex_index << 1) & 2, vertex_index & 2);
    output.position = f32x4(2.0 * output.tex_coord - 1.0, 1.0, 1.0);

    return output;
}

func draw_sun(f32x3 world_dir, f32x3 sun_dir, f32 radius) -> f32x3 {
    const let min_cos_theta = cos(radius * PI / 180.0);

    const let cosTheta = dot(world_dir, sun_dir);
    if (cosTheta >= min_cos_theta) {
        return 1.0;
    }

    const let offset = min_cos_theta - cosTheta;
    const let gaussianBloom = exp(-offset * 50000.0) * 0.5;
    const let invBloom = 1.0 / (0.02 + offset * 300.0) * 0.01;
    return f32x3(gaussianBloom + invBloom);
}

[[shader("fragment")]]
func fs_main(
    VertexOutput input,
    uniform ParameterBlock<ShaderParameters> params
) -> f32x4 {
    f32 depth = params.depth_image.sample_mip(params.sampler, input.tex_coord, 0.0);
    f32x3 NDC = f32x3(input.tex_coord * 2.0 - 1.0, depth);
    f32x4 world_pos_h = mul(params.camera.inv_projection_view_mat, f32x4(NDC, 1.0));
    f32x3 world_pos = world_pos_h.xyz / world_pos_h.w;
    f32x3 camera_pos = params.camera.position;

    if (depth != 0.0) {
        f32x3 camera_relative_pos = (world_pos - camera_pos) * CAMERA_SCALE_UNIT;
        return sample_aerial_perspective(
            params.sky_aerial_perspective_lut,
            params.sampler,
            params.environment.aerial_perspective_lut_size,
            input.tex_coord,
            camera_relative_pos,
            params.environment.atmos_aerial_perspective_start_km);
    }

    let eye_dir = normalize(world_pos - params.camera.position);
    var eye_altitude = params.camera.position.y * CAMERA_SCALE_UNIT;
    eye_altitude += params.environment.atmos_planet_radius + PLANET_RADIUS_OFFSET;
    let eye_pos = f32x3(0.0, eye_altitude, 0.0);
    let up = f32x3(0.0, 1.0, 0.0);
    let right = normalize(cross(up, eye_dir));
    let forward = normalize(cross(right, up));
    let sun_dir = normalize(params.environment.sun_direction);
    let light_on_plane = normalize(f32x2(dot(sun_dir, forward), dot(sun_dir, right)));

    f32 view_zenith_cos_angle = dot(eye_dir, up);

    const let planet_intersection = std::ray_sphere_intersect_nearest(eye_pos, eye_dir, params.environment.atmos_planet_radius);
    f32x2 uv = sky_view_params_to_lut_uv(
        params.environment.atmos_atmos_radius,
        params.environment.atmos_planet_radius,
        params.environment.sky_view_lut_size.xy,
        planet_intersection.hasValue,
        eye_altitude,
        view_zenith_cos_angle,
        light_on_plane);
    f32x4 result = params.sky_view_lut.sample_mip(params.sampler, uv, 0.0);
    f32x3 luminance = result.rgb;
    f32 transmittance = result.a;

    f32 sun_cos_theta = dot(sun_dir, up);
    f32x2 transmittance_uv = transmittance_params_to_lut_uv(
        params.environment.atmos_atmos_radius,
        params.environment.atmos_planet_radius,
        f32x2(eye_altitude, sun_cos_theta));
    f32x3 sun_transmittance = params.sky_transmittance_lut.sample_mip(params.sampler, transmittance_uv, 0.0).rgb;

    if (!planet_intersection.hasValue) {
        luminance += draw_sun(eye_dir, params.environment.sun_direction, 1.0) * params.environment.sun_intensity * sun_transmittance;
    }

    return f32x4(luminance, 1.0 - transmittance);
}
