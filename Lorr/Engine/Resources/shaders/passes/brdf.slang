module brdf;

import gpu;
import std;
import pbr;
import sky;
import scene;

#include <fullscreen.slang>

struct ShaderParameters {
    Sampler linear_clamp_sampler;
    Sampler linear_repeat_sampler;
    Image2D<f32x4> sky_transmittance_lut;
    Image2D<f32x4> sky_multiscattering_lut;
    Image2D<f32> depth_image;
    Image2D<f32x4> albedo_image;
    Image2D<f32x4> normal_image;
    Image2D<f32x3> emissive_image;
    Image2D<f32x3> metallic_roughness_occlusion_image;

    ConstantBuffer<Environment> environment;
    ConstantBuffer<Camera> camera;
};
ParameterBlock<ShaderParameters> params;

[[shader("fragment")]]
func fs_main(VertexOutput input) -> f32x4 {
    let pixel_pos = u32x3(u32x2(input.position.xy), 0);
    let depth = params.depth_image.Load(pixel_pos);
    if (depth == 0.0) {
        discard;
    }

    let albedo_color = params.albedo_image.SampleLevel(params.linear_repeat_sampler, input.tex_coord, 0).rgb;

    let mapped_smooth_normal = params.normal_image.Load(pixel_pos);
    let mapped_normal = std::oct_to_vec3(mapped_smooth_normal.xy);
    let smooth_normal = std::oct_to_vec3(mapped_smooth_normal.zw);

    let emission = params.emissive_image.Load(pixel_pos);

    let metallic_roughness_occlusion = params.metallic_roughness_occlusion_image.Load(pixel_pos);
    let metallic = metallic_roughness_occlusion.x;
    let roughness = metallic_roughness_occlusion.y;
    let occlusion = metallic_roughness_occlusion.z;

    let NDC = f32x3(input.tex_coord * 2.0 - 1.0, depth);
    let world_position_h = mul(params.camera.inv_projection_view_mat, f32x4(NDC, 1.0));
    let world_position = world_position_h.xyz / world_position_h.w;

    // PBR constants
    let V = normalize(params.camera.position - world_position);
    let L = normalize(params.environment.sun_direction); // temp
    let N = normalize(mapped_normal);

    var sun_illuminance = f32x3(1.0);
    var sky_luminance = f32x3(0.1);
    if (params.environment.flags & (EnvironmentFlags::HasSun | EnvironmentFlags::HasAtmosphere)) {
        // SUN LIGHT COLOR ──────────────────────────────────────────────────
        var eye_altitude = max(world_position.y, 0.0) * CAMERA_SCALE_UNIT;
        eye_altitude += params.environment.atmos_planet_radius + PLANET_RADIUS_OFFSET;
        var eye_pos = f32x3(0.0, eye_altitude, 0.0);
        let up_vec = f32x3(0.0, 1.0, 0.0);
        f32 sun_cos_theta = dot(L, up_vec);
        f32x2 transmittance_uv = transmittance_params_to_lut_uv(
            params.environment.atmos_atmos_radius,
            params.environment.atmos_planet_radius,
            f32x2(eye_altitude, sun_cos_theta));
        f32x3 sun_transmittance = params.sky_transmittance_lut.SampleLevel(params.linear_clamp_sampler, transmittance_uv, 0.0).rgb;
        sun_illuminance = sun_transmittance * params.environment.sun_intensity;

        // SKY AMBIENT COLOR ────────────────────────────────────────────────
        AtmosphereIntegrateInfo sky_info = {};
        sky_info.eye_pos = eye_pos;
        sky_info.eye_dir = up_vec;
        sky_info.sun_dir = L;
        sky_info.sun_intensity = params.environment.sun_intensity;
        sky_info.sampling.variable_sample_count = true;
        sky_info.sampling.min_sample_count = 1;
        sky_info.sampling.max_sample_count = 4;
        sky_info.eval_multiscattering = true;
        sky_info.eval_mie_phase = false;
        let sky_result = integrate_single_scattered_luminance(
            sky_info, params.environment, params.linear_clamp_sampler, params.sky_transmittance_lut, params.sky_multiscattering_lut);

        var eye_gradient = dot(N, up_vec);
        eye_gradient = (eye_gradient + 1.0) * 0.375 + 0.25;
        sky_luminance = std::rec709_oetf(sky_result.luminance) * eye_gradient;
    }

    let ambient_contribution = sky_luminance * albedo_color * occlusion;

    // MATERIAL COLOR ───────────────────────────────────────────────────
    // https://marmosetco.tumblr.com/post/81245981087
    let R = reflect(-V, N);
    let horizon_fade = 1.3;
    var horizon = saturate(1.0 + horizon_fade * dot(R, smooth_normal));
    horizon *= horizon;

    var material_surface_color = f32x3(0.0);
    let NoL = max(dot(N, L), 0.0);
    if (NoL > 0.0) {
        let brdf = BRDF(V, N, L, albedo_color, roughness, metallic);
        material_surface_color = brdf * horizon * sun_illuminance * NoL;
    }

    // FINAL ────────────────────────────────────────────────────────────
    let final_color = material_surface_color + ambient_contribution + emission;

    return f32x4(final_color, 1.0);
}
