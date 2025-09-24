import gpu;
import std;
import pbr;
import sky;
import scene;
import shadow;

#include <fullscreen.slang>

struct ShaderParameters {
    Sampler linear_clamp_sampler;
    Sampler linear_repeat_sampler;
    Image2D<f32x4> sky_transmittance_lut;
    TextureCube<f32x4> sky_cubemap;
    Image2D<f32> depth_image;
    Image2D<f32> ambient_occlusion;
    Image2D<f32x4> albedo_image;
    Image2D<f32x4> normal_image;
    Image2D<f32x3> emissive_image;
    Image2D<f32x3> metallic_roughness_occlusion_image;
    Image2D<f32> vsm_page_table;

    ConstantBuffer<Camera> camera;
};
ParameterBlock<ShaderParameters> params;

[[shader("fragment")]]
func fs_main(
    VertexOutput input,
    uniform PBRContext context
) -> f32x4 {
    let pixel_pos = u32x3(u32x2(input.position.xy), 0);
    let depth = params.depth_image.Load(pixel_pos);
    if (depth == 0.0) {
        discard;
    }

    let albedo_color = params.albedo_image.SampleLevel(params.linear_repeat_sampler, input.tex_coord, 0).rgb;

    let mapped_smooth_normal = params.normal_image.Load(pixel_pos);
    let mapped_normal = std::octahedral_decode(mapped_smooth_normal.rg);
    let smooth_normal = std::octahedral_decode(mapped_smooth_normal.ba);

    let emission = params.emissive_image.Load(pixel_pos);

    let metallic_roughness_occlusion = params.metallic_roughness_occlusion_image.Load(pixel_pos);
    let metallic = metallic_roughness_occlusion.r;
    let roughness = metallic_roughness_occlusion.g;
    let baked_occlusion = metallic_roughness_occlusion.b;
    let screen_space_occlusion = params.ambient_occlusion.Load(pixel_pos).r;
    let occlusion = baked_occlusion * screen_space_occlusion;

    let NDC = f32x3(input.tex_coord * 2.0 - 1.0, depth);
    let world_position_h = mul(params.camera.inv_projection_view_mat, f32x4(NDC, 1.0));
    let world_position = world_position_h.xyz / world_position_h.w;

    // PBR constants
    let Li = context.directional_light ? context.directional_light.intensity : 1.0;
    let L = context.atmosphere ? context.atmosphere.sun_direction : f32x3(0.0, 1.0, 0.0);
    let V = normalize(params.camera.position - world_position);
    let N = normalize(mapped_normal);
    let R = reflect(-V, N);

    var indirect_illuminance = f32x3(0.0);
    var sun_illuminance = f32x3(1.0);
    if (context.atmosphere) {
        let planet_radius = context.atmosphere.planet_radius;
        let atmos_radius = context.atmosphere.atmos_radius;

        var eye_altitude = max(world_position.y, 0.0) * CAMERA_SCALE_UNIT;
        eye_altitude += planet_radius + PLANET_RADIUS_OFFSET;

        f32 sun_cos_theta = dot(L, f32x3(0.0, 1.0, 0.0));
        let horizon_darkening = smoothstep(-0.1, 0.3, sun_cos_theta);

        f32x2 transmittance_uv = transmittance_params_to_lut_uv(
            atmos_radius,
            planet_radius,
            f32x2(eye_altitude, sun_cos_theta));
        f32x3 sun_transmittance = params.sky_transmittance_lut.SampleLevel(params.linear_clamp_sampler, transmittance_uv, 0.0).rgb;
        sun_illuminance = sun_transmittance * Li * horizon_darkening;

        let reflection_dir = lerp(N, R, 1.0 - roughness);
        let specular_sample = params.sky_cubemap.SampleLevel(params.linear_clamp_sampler, reflection_dir, 0.0);
        let diffuse_sample = params.sky_cubemap.SampleLevel(params.linear_clamp_sampler, N, 0.0);

        let specular_sky = specular_sample.rgb * specular_sample.a;
        let diffuse_sky = diffuse_sample.rgb * diffuse_sample.a;

        let NoV = abs(dot(N, V)) + 1e-5;
        let reflectance = 0.04;
        let F0 = lerp(reflectance, albedo_color, metallic);
        let F = F_Schlick(NoV, F0);
        let kD = (1.0 - metallic) * (1.0 - F);
        let diffuse = kD * diffuse_sky * albedo_color * Fd_Lambert();

        let specular_strength = (1.0 - roughness) * (1.0 - roughness);
        let specular = F * specular_sky * specular_strength;
        indirect_illuminance = diffuse + specular;
    }

    // MATERIAL COLOR ───────────────────────────────────────────────────
    // https://marmosetco.tumblr.com/post/81245981087
    let horizon_fade = 1.3;
    var horizon = saturate(1.0 + horizon_fade * dot(R, smooth_normal));
    horizon *= horizon;

    var material_surface_color = f32x3(0.0);
    let NoL = max(dot(N, L), 0.0);
    if (NoL > 0.0) {
        var directional_shadow = 1.0;
        // if (context.directional_light_cascades) {
        //     let view_z = -mul(params.camera.view_mat, f32x4(world_position, 1.0)).z;
        //     directional_shadow = sample_shadow_map(
        //         context.directional_light,
        //         context.directional_light_cascades,
        //         params.directional_light_shadowmap,
        //         params.directional_light_sampler,
        //         view_z, world_position, N);
        // }

        let brdf = BRDF(V, N, L, albedo_color, roughness, metallic);
        material_surface_color = brdf * horizon * sun_illuminance * NoL * directional_shadow;
    }

    // FINAL ────────────────────────────────────────────────────────────
    let base_ambient_color = context.directional_light ? context.directional_light.base_ambient_color : f32x3(0.02);
    let ambient_contribution = base_ambient_color * albedo_color * sun_illuminance;
    let indirect_illuminance_sum = (indirect_illuminance + ambient_contribution) * occlusion;
    let final_color = material_surface_color + indirect_illuminance_sum + emission;

    return f32x4(final_color, 1.0);
}
