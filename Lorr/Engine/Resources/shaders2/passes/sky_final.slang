module sky_view;

import std;
import gpu;
import sky;
import scene;

struct PushConstants {
    Sampler sampler;
    Image2D<f32x4> sky_transmittance_lut;
    Image2D<f32x4> sky_multiscattering_lut;
    Image3D<f32x4> sky_aerial_perspective_lut;
    Image2D<f32x4> sky_view_lut;
    Image2D<f32>   depth_image;
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
    f32   depth = C.depth_image.load(u32x2(input.position.xy));
    f32x3 clip_space = f32x3(input.tex_coord * 2.0 - 1.0, depth);
    f32x4 clip_pos = mul(C.camera->inv_projection_view_mat, f32x4(clip_space, 1.0));
    f32x3 clip_dir = clip_pos.xyz / clip_pos.w;

    f32x3 world_dir = normalize(clip_dir - C.camera->position);
    f32x3 world_pos = f32x3(0.0, C.camera->position.y, 0.0) *
                       CAMERA_SCALE_UNIT +
                       f32x3(0.0, C.atmosphere->planet_radius, 0.0);

    if (depth != 1.0) {
        f32x3 camera_relative_pos = (clip_dir - C.camera->position) * CAMERA_SCALE_UNIT;
        return sample_aerial_perspective(C.atmosphere, C.sky_aerial_perspective_lut, C.sampler, input.tex_coord, camera_relative_pos);
    }

    f32 h = length(world_pos);
    f32x3 up_vec = normalize(world_pos);

    f32x3 sun_dir = normalize(C.sun->direction);
    f32x3 right_vec = normalize(cross(up_vec, world_dir));
    f32x3 forward_vec = normalize(cross(right_vec, up_vec));
    f32x2 light_on_plane = normalize(f32x2(dot(sun_dir, forward_vec), dot(sun_dir, right_vec)));

    f32 view_zenith_cos_angle = dot(world_dir, up_vec);
    f32 light_view_cos_angle = light_on_plane.x;

    const let planet_intersection = std::ray_sphere_intersect_nearest(world_pos, world_dir, C.atmosphere->planet_radius);
    f32x2 uv = get_sky_view_lut_uv(C.atmosphere, planet_intersection.hasValue, view_zenith_cos_angle, light_view_cos_angle, h);
    f32x3 luminance = C.sky_view_lut.sample_mip(C.sampler, uv, 0.0).rgb;

    f32 sun_cos_theta = dot(C.sun->direction, up_vec);
    f32x2 transmittance_uv = get_transmittance_lut_uv(C.atmosphere, f32x2(h, sun_cos_theta));
    f32x3 sun_transmittance = C.sky_transmittance_lut.sample_mip(C.sampler, transmittance_uv, 0.0).rgb;

    if (!planet_intersection.hasValue) {
        luminance += draw_sun(world_dir, C.sun->direction, 1.0) * C.sun->intensity * sun_transmittance;
    }

    return f32x4(luminance, 1.0);
}

