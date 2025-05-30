module brdf;

import gpu;
import std;
import pbr;
import sky;
import scene;

[[vk::binding(0, 0)]]
Sampler linear_clamp_sampler;

[[vk::binding(1, 0)]]
Sampler linear_repeat_sampler;

[[vk::binding(2, 0)]]
Image2D<f32x4> sky_transmittance_lut;

[[vk::binding(3, 0)]]
Image2D<f32x4> sky_multiscattering_lut;

[[vk::binding(4, 0)]]
Image2D<f32> depth_image;

[[vk::binding(5, 0)]]
Image2D<f32x4> albedo_image;

[[vk::binding(6, 0)]]
Image2D<f32x4> normal_image;

[[vk::binding(7, 0)]]
Image2D<f32x3> emissive_image;

[[vk::binding(8, 0)]]
Image2D<f32x3> metallic_roughness_occlusion_image;

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
VertexOutput vs_main(u32 vertex_id : SV_VertexID) {
    VertexOutput output;
    output.tex_coord = f32x2((vertex_id << 1) & 2, vertex_id & 2);
    output.position = f32x4(2.0 * output.tex_coord - 1.0, 1.0, 1.0);

    return output;
}

[[shader("fragment")]]
f32x4 fs_main(VertexOutput input) : SV_Target {
    const u32x2 pixel_pos = u32x2(input.position.xy);
    const f32 depth = depth_image.load(pixel_pos);
    if (depth == 1.0) {
        discard;
    }

    const f32x3 albedo_color = albedo_image.sample_mip(linear_repeat_sampler, input.tex_coord, 0).rgb;
    
    const f32x4 mapped_smooth_normal = normal_image.load(pixel_pos);
    const f32x3 mapped_normal = std::oct_to_vec3(mapped_smooth_normal.xy);
    const f32x3 smooth_normal = std::oct_to_vec3(mapped_smooth_normal.zw);

    const f32x3 emission = emissive_image.load(pixel_pos);
    
    const f32x3 metallic_roughness_occlusion = metallic_roughness_occlusion_image.load(pixel_pos);
    const f32  metallic = metallic_roughness_occlusion.x;
    const f32  roughness = metallic_roughness_occlusion.y;
    const f32  occlusion = metallic_roughness_occlusion.z;

    const f32x4 NDC = f32x4(input.tex_coord * 2.0 - 1.0, depth, 1.0);
    const f32x4 world_position = mul(C.camera->inv_projection_view_mat, NDC);
    const f32x3 ray_pos = world_position.xyz / world_position.w;

    // PBR constants
    const f32x3 V = normalize(C.camera->position - ray_pos);
    const f32x3 L = normalize(C.sun->direction);  // temp
    const f32x3 N = normalize(mapped_normal);

    // SUN LIGHT COLOR ──────────────────────────────────────────────────
    const f32x3 ray_pos_planet = f32x3(0.0, ray_pos.y, 0.0) *
                            CAMERA_SCALE_UNIT +
                            f32x3(0.0, C->atmosphere.planet_radius, 0.0);
    f32 h = length(ray_pos_planet);
    f32x3 up_vec = normalize(ray_pos_planet);
    f32 sun_cos_theta = dot(L, up_vec);
    f32x2 transmittance_uv = transmittance_params_to_lut_uv(C.atmosphere, f32x2(h, sun_cos_theta));
    f32x3 sun_transmittance = sky_transmittance_lut.sample_mip(linear_clamp_sampler, transmittance_uv, 0.0).rgb;
    const let planet_intersection = std::ray_sphere_intersect_nearest(ray_pos_planet, L, C.atmosphere->planet_radius);
    if (planet_intersection.hasValue) {
        // TODO: Implement a penumbra
        sun_transmittance = 0.0;
    }

    // SKY AMBIENT COLOR ────────────────────────────────────────────────
    AtmosphereIntegrateInfo sky_info = {};
    sky_info.eye_pos = ray_pos_planet;
    sky_info.sun_dir = L;
    sky_info.sun_intensity = C.sun->intensity;
    sky_info.sampling.variable_sample_count = true;
    sky_info.sampling.min_sample_count = 1;
    sky_info.sampling.max_sample_count = 4;
    sky_info.transmittance_image = sky_transmittance_lut;
    sky_info.multiscattering_image = sky_multiscattering_lut;
    sky_info.eval_mie_phase = false;

    sky_info.eye_dir = f32x3(0.0, -1.0, 0.0);
    const let sky_result_0 = integrate_single_scattered_luminance(C.atmosphere, linear_clamp_sampler, sky_info);
    sky_info.eye_dir = f32x3(0.0, +1.0, 0.0);
    const let sky_result_1 = integrate_single_scattered_luminance(C.atmosphere, linear_clamp_sampler, sky_info);
    const let sky_gradient = lerp(
        sky_result_0.luminance,
        sky_result_1.luminance,
        saturate(N.y * 0.5 + 0.5));
    const let ambient_color = std::rec709_oetf(sky_gradient);
    f32x3 color = albedo_color * ambient_color;

    // MATERIAL COLOR ───────────────────────────────────────────────────
    // https://marmosetco.tumblr.com/post/81245981087
    const f32x3 R = reflect(-V, N);
    const f32 horizon_fade = 1.3;
    f32 horizon = saturate(1.0 + horizon_fade * dot(R, smooth_normal));
    horizon *= horizon;

    const f32 NoL = max(dot(N, L), 0.0);
    f32x3 brdf = BRDF(V, N, L, albedo_color, roughness, metallic);
    color += brdf * horizon * sun_transmittance * C.sun->intensity * NoL;
    color += emission;

    color *= occlusion;

    return f32x4(color, 1.0);
}

