module sky_view;

import std;
import gpu;
import sky;
import scene;

[[vk::binding(0, 0)]]
Sampler sampler;
[[vk::binding(1, 0)]]
Image2D<f32x4> sky_transmittance_lut;
[[vk::binding(2, 0)]]
Image3D<f32x4> sky_aerial_perspective_lut;
[[vk::binding(3, 0)]]
Image2D<f32x4> sky_view_lut;
[[vk::binding(4, 0)]]
Image2D<f32> depth_image;

struct PushConstants {
    Atmosphere *atmosphere;
    Sun *sun;
    Camera *camera;
};
[[vk::push_constant]] PushConstants C;

struct VertexOutput {
    f32x4 position : SV_Position;
    f32x2 tex_coord : TEXCOORD;
};

[[shader("vertex")]]
func vs_main(u32 vertex_index : SV_VertexID) -> VertexOutput{
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
    const let invBloom = 1.0/(0.02 + offset * 300.0) * 0.01;
    return f32x3(gaussianBloom + invBloom);
}

[[shader("fragment")]]
f32x4 fs_main(VertexOutput input) : SV_TARGET {
    f32   depth = depth_image.sample_mip(sampler, input.tex_coord, 0.0);
    f32x3 clip_space = f32x3(input.tex_coord * 2.0 - 1.0, depth);
    f32x4 clip_pos = mul(C.camera->inv_projection_view_mat, f32x4(clip_space, 1.0));
    f32x3 clip_dir = clip_pos.xyz / clip_pos.w;

    const f32x3 sun_dir = normalize(C.sun->direction);
    f32x3 world_dir = normalize(clip_dir - C.camera->position);
    f32x3 world_pos = C.camera->position * CAMERA_SCALE_UNIT;
    world_pos.y += C.atmosphere->planet_radius + PLANET_RADIUS_OFFSET;

    if (depth != 1.0) {
        f32x3 camera_relative_pos = (clip_dir - C.camera->position) * CAMERA_SCALE_UNIT;
        return sample_aerial_perspective(C.atmosphere, sky_aerial_perspective_lut, sampler, input.tex_coord, camera_relative_pos);
    }

    f32 h = length(world_pos);
    f32x3 up = f32x3(0.0, 1.0, 0.0);
    f32x3 right = normalize(cross(up, world_dir));
    f32x3 forward = normalize(cross(right, up));
    f32x2 light_on_plane = normalize(f32x2(dot(sun_dir, forward), dot(sun_dir, right)));

    f32 view_zenith_cos_angle = dot(world_dir, up);

    const let planet_intersection = std::ray_sphere_intersect_nearest(world_pos, world_dir, C.atmosphere->planet_radius);
    f32x2 uv = sky_view_params_to_lut_uv(C.atmosphere, planet_intersection.hasValue, h, view_zenith_cos_angle, light_on_plane);
    f32x4 result = sky_view_lut.sample_mip(sampler, uv, 0.0);
    f32x3 luminance = result.rgb;
    f32 transmittance = result.a;

    f32 sun_cos_theta = dot(sun_dir, up);
    f32x2 transmittance_uv = transmittance_params_to_lut_uv(C.atmosphere, f32x2(h, sun_cos_theta));
    f32x3 sun_transmittance = sky_transmittance_lut.sample_mip(sampler, transmittance_uv, 0.0).rgb;

    if (!planet_intersection.hasValue) {
        luminance += draw_sun(world_dir, C.sun->direction, 1.0) * C.sun->intensity * sun_transmittance;
    }

    return f32x4(luminance, 1.0 - transmittance);
}

