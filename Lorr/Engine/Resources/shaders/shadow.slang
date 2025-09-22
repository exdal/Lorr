module shadow;

import std;
import scene;

#ifndef MAX_DIRECTIONAL_SHADOW_CASCADES
#define MAX_DIRECTIONAL_SHADOW_CASCADES 4
#endif

// TODO: Move this to color, yes i could actually do this instead of writing this text
// but i am SO FUCKING LAZY
func hsv_to_rgb(f32x3 hsv) -> f32x3 {
    let n = f32x3(5.0, 3.0, 1.0);
    let k = (n + hsv.x / 1.0471975512) % 6.0;
    return hsv.z - hsv.z * hsv.y * max(f32x3(0.0), min(k, min(4.0 - k, f32x3(1.0))));
}

public func cascade_debug_visualization(
    f32x3 output_color,
    u32 cascade_index,
    f32 view_z
) -> f32x3 {
    let overlay_alpha = 0.70;
    let cascade_color_hsv = f32x3(
        f32(cascade_index) / f32(MAX_DIRECTIONAL_SHADOW_CASCADES + 1u) * TAU,
        1.0,
        0.5
    );
    let cascade_color = hsv_to_rgb(cascade_color_hsv);
    return f32x3(
        (1.0 - overlay_alpha) * output_color.rgb + overlay_alpha * cascade_color
    );
}

public func get_cascade_index(DirectionalLightCascade *cascades, u32 cascade_count, f32 view_z) -> u32 {
    for (var i = 0u; i < cascade_count; i++) {
        if (view_z < cascades[i].far_bound) {
            return i;
        }
    }

    return cascade_count - 1;
}

func get_cascade_sample_info(
    in DirectionalLightCascade cascade,
    f32x3 world_position,
    f32x3 normal,
    f32x3 light_direction,
    f32 normal_bias,
    f32 depth_bias
) -> f32x4 {
    let normal_offset = normal_bias * cascade.texel_size * normal;
    let depth_offset = depth_bias * light_direction;
    let offset_position = f32x3(world_position.xyz + normal_offset + depth_offset);
    let offset_position_clip = mul(cascade.projection_view_mat, f32x4(offset_position, 1.0));
    if (offset_position_clip.w <= 0.0) {
        return 0.0;
    }

    let offset_position_ndc = offset_position_clip.xyz / offset_position_clip.w;
    // No shadow outside the orthographic projection volume
    if (any(offset_position_ndc.xy < -1.0) ||
        any(offset_position_ndc.xy > 1.0) ||
        offset_position_ndc.z < 0.0 ||
        offset_position_ndc.z > 1.0) {
        return 0.0;
    }

    return f32x4(offset_position_ndc.xy * 0.5 + 0.5, offset_position_ndc.z, 1.0);
}

public func sample_shadow_map(
    DirectionalLight *light,
    DirectionalLightCascade *cascades,
    Texture2DArray<f32> shadow_map,
    SamplerState shadow_sampler,
    f32 view_z,
    f32x3 world_position,
    f32x3 normal,
    f32x3 light_direction
) -> f32 {
    let cascade_index = get_cascade_index(cascades, light.cascade_count, view_z);
    let this_cascade = cascades[cascade_index];
    let this_cascade_sample_info = get_cascade_sample_info(
        this_cascade,
        world_position,
        normal,
        light_direction,
        light.normal_bias,
        light.depth_bias);

    let shadow_depth = shadow_map.SampleLevel(shadow_sampler, f32x3(this_cascade_sample_info.xy, cascade_index), 0.0);
    var shadow = this_cascade_sample_info.z < shadow_depth ? 0.0 : 1.0;

    let next_cascade_index = cascade_index + 1u;
    if (next_cascade_index < light.cascade_count) {
        let this_far_bound = this_cascade.far_bound;
        let next_near_bound = (1.0 - light.cascades_overlap_proportion) * this_far_bound;
        if (view_z >= next_near_bound) {
            let next_cascade = cascades[next_cascade_index];
            let next_cascade_sample_info = get_cascade_sample_info(
                next_cascade,
                world_position,
                normal,
                light_direction,
                light.normal_bias,
                light.depth_bias);

            let next_shadow_depth = shadow_map.SampleLevel(shadow_sampler, f32x3(next_cascade_sample_info.xy, next_cascade_index), 0.0);
            let next_cascade_shadow = next_cascade_sample_info.z < next_shadow_depth ? 0.0 : 1.0;
            shadow = lerp(shadow, next_cascade_shadow, (view_z - next_near_bound) / (this_far_bound - next_near_bound));
        }
    }
    return shadow;
}