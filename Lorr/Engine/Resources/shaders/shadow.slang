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

public func get_cascade_index(in DirectionalLight light, f32 view_z) -> u32 {
    for (var i = 0u; i < light.cascade_count; i++) {
        if (view_z < light.cascades[i].far_bound) {
            return i;
        }
    }

    return light.cascade_count - 1;
}

func sample_cascade(
    in DirectionalLight light,
    Texture2DArray<f32> shadow_map,
    SamplerState shadow_sampler,
    f32x3 world_position,
    f32x3 normal,
    f32x3 light_direction,
    u32 cascade_index
) -> f32 {
    let cascade = light.cascades[cascade_index];
    let normal_offset = light.normal_bias * cascade.texel_size * normal;
    let depth_offset = light.depth_bias * light_direction;
    let offset_position = f32x3(world_position.xyz + normal_offset + depth_offset);
    let offset_position_clip = mul(cascade.projection_view_mat, f32x4(offset_position, 1.0));
    if (offset_position_clip.w <= 0.0) {
        return 1.0;
    }

    let offset_position_ndc = offset_position_clip.xyz / offset_position_clip.w;
    // No shadow outside the orthographic projection volume
    if (any(offset_position_ndc.xy < -1.0) ||
        any(offset_position_ndc.xy > 1.0) ||
        offset_position_ndc.z < 0.0 ||
        offset_position_ndc.z > 1.0) {
        return 1.0;
    }

    let light_local = f32x3(offset_position_ndc.xy * 0.5 + 0.5, offset_position_ndc.z);
    let shadow_depth = shadow_map.SampleLevel(shadow_sampler, f32x3(light_local.xy, cascade_index), 0.0);

    return light_local.z < shadow_depth ? 0.0 : 1.0;
}

public func sample_shadow_map(
    in DirectionalLight light,
    Texture2DArray<f32> shadow_map,
    SamplerState shadow_sampler,
    f32 view_z,
    f32x3 world_position,
    f32x3 normal,
    f32x3 light_direction
) -> f32 {
    let cascade_index = get_cascade_index(light, view_z);

    var shadow = sample_cascade(
        light,
        shadow_map,
        shadow_sampler,
        world_position,
        normal,
        light_direction,
        cascade_index);

    let next_cascade_index = cascade_index + 1u;
    if (next_cascade_index < light.cascade_count) {
        let this_far_bound = light.cascades[cascade_index].far_bound;
        let next_near_bound = (1.0 - light.cascades_overlap_proportion) * this_far_bound;
        if (view_z >= next_near_bound) {
            var next_shadow = sample_cascade(
                light,
                shadow_map,
                shadow_sampler,
                world_position,
                normal,
                light_direction,
                next_cascade_index);

            shadow = lerp(shadow, next_shadow, (view_z - next_near_bound) / (this_far_bound - next_near_bound));
        }
    }
    return shadow;
}