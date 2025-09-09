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
    let min_cos_theta = cos(radius * PI / 250.0);
    let cosTheta = dot(world_dir, sun_dir);
    return cosTheta >= min_cos_theta ? 1.0 : 0.0;
}

func build_orthonormal_basis(f32x3 z_basis) -> f32x3x3 {
    let sign = copysign(1.0, z_basis.z);
    let a = -1.0 / (sign + z_basis.z);
    let b = z_basis.x * z_basis.y * a;
    let x_basis = f32x3(1.0 + sign * z_basis.x * z_basis.x * a, sign * b, -sign * z_basis.x);
    let y_basis = f32x3(b, sign + z_basis.y * z_basis.y * a, -z_basis.y);
    return f32x3x3(x_basis, y_basis, z_basis);
}

[[shader("fragment")]]
func fs_main(
    VertexOutput input,
    uniform ParameterBlock<ShaderParameters> params
) -> f32x4 {
    f32 depth = params.depth_image.SampleLevel(params.sampler, input.tex_coord, 0.0);
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

    let up = f32x3(0.0, 1.0, 0.0);
    var eye_altitude = params.camera.position.y * CAMERA_SCALE_UNIT;
    eye_altitude += params.environment.atmos_planet_radius + PLANET_RADIUS_OFFSET;
    let eye_pos = f32x3(0.0, eye_altitude, 0.0);

    var sun_dir = (params.environment.sun_direction);
    var eye_dir = normalize(world_pos - params.camera.position);

    let view_zenith_cos_angle = acos(dot(eye_dir, up));
    let light_view_cos_angle = acos(clamp(dot(
        normalize(f32x3(sun_dir.x, 0.0, sun_dir.z)),
        normalize(f32x3(eye_dir.x, 0.0, eye_dir.z)),
    ), -1.0, 1.0));
    let planet_intersection = std::ray_sphere_intersect_nearest(eye_pos, eye_dir, params.environment.atmos_planet_radius);
    let uv = sky_view_params_to_lut_uv(
        params.environment.atmos_atmos_radius,
        params.environment.atmos_planet_radius,
        params.environment.sky_view_lut_size.xy,
        planet_intersection != -1.0,
        eye_altitude,
        view_zenith_cos_angle,
        light_view_cos_angle);

    let result = params.sky_view_lut.SampleLevel(params.sampler, uv, 0.0);
    let atmos_luminance = result.rgb * result.a;
    var color = atmos_luminance * params.environment.sun_intensity;

    let sun_cos_theta = dot(sun_dir, up);
    let transmittance_uv = transmittance_params_to_lut_uv(
        params.environment.atmos_atmos_radius,
        params.environment.atmos_planet_radius,
        f32x2(eye_altitude, sun_cos_theta));
    let sun_transmittance = params.sky_transmittance_lut.SampleLevel(params.sampler, transmittance_uv, 0.0).rgb;

    if (planet_intersection == -1.0) {
        color += draw_sun(eye_dir, params.environment.sun_direction, 1.0) * params.environment.sun_intensity * sun_transmittance;
    }

    return f32x4(color, 1.0);
}
