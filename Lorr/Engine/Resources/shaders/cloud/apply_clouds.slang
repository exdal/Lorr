// Sources used to achieve this shader:
// - https://advances.realtimerendering.com/s2023/Nubis%20Cubed%20%28Advances%202023%29.pdf
// - https://advances.realtimerendering.com/s2022/SIGGRAPH2022-Advances-NubisEvolved-NoVideos.pdf
// - https://github.com/MatejSakmary/atmosphere-bac-text
// - https://www.slideshare.net/guerrillagames/the-realtime-volumetric-cloudscapes-of-horizon-zero-dawn
// - https://www.ea.com/frostbite/news/physically-based-sky-atmosphere-and-cloud-rendering
// - https://sebh.github.io/publications/egsr2020.pdf
// - https://www.youtube.com/watch?v=zs0oYjwjNEo&t=4620s

import cloud;
import "../atmos/atmos";
import "../lorr";

[[vk::binding(0, 0)]] SamplerState linear_repeat_sampler;
[[vk::binding(1, 0)]] SamplerState linear_clamp_to_edge_sampler;
[[vk::binding(2, 0)]] Texture3D<float4> shape_noise_lut;
[[vk::binding(3, 0)]] Texture3D<float4> detail_noise_lut;
[[vk::binding(4, 0)]] Texture2D<float4> transmittance_lut;
[[vk::binding(5, 0)]] Texture3D<float4> aerial_perspective_lut;
[[vk::binding(6, 0)]] ConstantBuffer<Scene, ScalarDataLayout> scene;

static const f32 DENSITY_EPSILON = 1e-4;

float bayer2(vec2 a) {
    a = floor(a);
    return fract(dot(a, vec2(0.5, a.y * 0.75)));
}

#define bayer4(a)   (bayer2(0.5   * (a))  *  0.25 + bayer2(a))
#define bayer8(a)   (bayer4(0.5   * (a))  *  0.25 + bayer2(a))
#define bayer16(a)  (bayer8(0.5   * (a))  *  0.25 + bayer2(a))
#define bayer32(a)  (bayer16(0.5  * (a))  *  0.25 + bayer2(a))
#define bayer64(a)  (bayer32(0.5  * (a))  *  0.25 + bayer2(a))

struct VertexOutput {
    vec4 position  : SV_Position;
    vec2 tex_coord : TEXCOORD;
};

[shader("vertex")]
VertexOutput vs_main(u32 vertex_id : SV_VertexID) {
    VertexOutput output;

    output.tex_coord = vec2((vertex_id << 1) & 2, vertex_id & 2);
    output.position = vec4(output.tex_coord * 2.0 - 1.0, 0.5, 1);

    return output;
}

// TODO: Weathers
// TODO: Coverage map
// TODO: Wetness
// TODO: Different cloud types
func sample_density(float3 pos_m) -> float {
    const let atmos = scene.atmosphere;
    const let clouds = scene.clouds;

    const float cloud_layer_min_km = clouds.bounds.x;
    const float cloud_layer_max_km = clouds.bounds.y;
    const float cloud_layer_thickness_km = cloud_layer_max_km - cloud_layer_min_km;

    float H = length(pos_m * CAMERA_SCALE_UNIT);
    float height_km = H - atmos.planet_radius;
    float height_norm = saturate((height_km - cloud_layer_min_km) - cloud_layer_thickness_km);

    float3 shape_uvw = pos_m * clouds.shape_noise_scale * 1e-6;
    float4 shape_noise = shape_noise_lut.Sample(linear_repeat_sampler, shape_uvw);
    float shape_noise_fbm = dot(shape_noise, normalize(clouds.shape_noise_weights));

    float base_density = saturate(remap(shape_noise.x, 1.0 - shape_noise_fbm, 1.0, 0.0, 1.0));
    base_density = remap(base_density, 1.0 - clouds.coverage, 1.0, 0.0, 1.0);
    base_density *= clouds.coverage;

    //float vertical_density = detail_noise_lut.Sample(sampler, float3(clouds.cloud_type, height_norm, 0)).w;
    //base_density *= vertical_density;

    float3 detail_uvw = pos_m * clouds.detail_noise_scale * 1e-6;
    float4 detail_sample = detail_noise_lut.Sample(linear_repeat_sampler, detail_uvw);
    float detail_fbm = dot(detail_sample, clouds.detail_noise_weights);

    float one_minus_shape = 1.0 - base_density;
    float detail_erode_weight = one_minus_shape * one_minus_shape * one_minus_shape;
    float final_cloud = base_density - (1.0 - detail_fbm) * detail_erode_weight * clouds.detail_noise_influence;
    return final_cloud * clouds.general_density;
}

func sample_sun_density(float3 pos_km) -> float {
    const let atmos = scene.atmosphere;
    const let clouds = scene.clouds;
    const let sun = scene.sun;

    float3 cloud_layer_boundaries = get_cloud_layer_boundaries(atmos, clouds, pos_km, sun.direction);
    const let march_dist = min(cloud_layer_boundaries.y - cloud_layer_boundaries.x, 20.0);
    const let step_count = float(clouds.sun_step_count);

    float old_ray_shift = 0.0;
    float density_sum = 0.0;
    for (int i = 0; i < step_count; i++) {
        float step_0 = float(i) / step_count;
        step_0 *= step_0;
        float step_1 = float(i + 1) / step_count;
        step_1 *= step_1;

        step_0 *= march_dist;
        step_1 = step_1 > 1.0 ? march_dist : step_1 * march_dist;
        float new_ray_shift = step_0 + (step_1 - step_0) * 0.3;
        float march_step = step_1 - step_0;
        old_ray_shift = new_ray_shift;

        float3 step_pos = new_ray_shift * sun.direction + pos_km;
        float3 step_pos_m = step_pos * INV_CAMERA_SCALE_UNIT;

        density_sum += max(0.0, sample_density(step_pos_m));
    }

    float transmittance = exp(-density_sum * clouds.extinction_coeff);
    return clouds.darkness_threshold + transmittance * (1.0 - clouds.darkness_threshold);
}

float dual_cloud_phase(float cos_theta, float3 params) {
    float forwards_lobe =  lr::henyey_greenstein_phase(params.x, cos_theta);
    float backwards_lobe = lr::henyey_greenstein_phase(params.y, cos_theta);
    float forwards_peak =  lr::henyey_greenstein_phase(params.z, cos_theta);
    return lerp(lerp(forwards_lobe, backwards_lobe, 0.20), forwards_peak, 0.15);
}

[shader("fragment")]
float4 fs_main(VertexOutput input) : SV_Target {
    const let camera = scene.camera;
    const let atmos = scene.atmosphere;
    const let clouds = scene.clouds;
    const let sun = scene.sun;
    const let clip_space = float3(input.tex_coord * 2.0 - 1.0, 1.0);
    const let world_space = mul(camera.inv_projection_view_mat, float4(clip_space, 1.0));
    const let world_pos = world_space.xyz / world_space.w;
    const let eye_dir = normalize(world_pos - camera.position);
    const let eye_pos = camera.position * CAMERA_SCALE_UNIT +
                        float3(0.0, atmos.planet_radius, 0.0);

    const let draw_distance = clouds.draw_distance * CAMERA_SCALE_UNIT;
    float3 cloud_layer_boundaries = get_cloud_layer_boundaries(atmos, clouds, eye_pos, eye_dir);
    cloud_layer_boundaries.xy = max(cloud_layer_boundaries.xy, 0.0);
    if (cloud_layer_boundaries.z == 1.0 ||
        cloud_layer_boundaries.y < cloud_layer_boundaries.x ||
        cloud_layer_boundaries.x > draw_distance) {
        return float4(0.0, 0.0, 0.0, 1.0);
    }

    float world_depth = length(world_pos - eye_pos);
    float march_dist = min(world_depth - cloud_layer_boundaries.x, cloud_layer_boundaries.y);
    float offset = bayer64(input.tex_coord);
    float3 start_pos = eye_pos + (cloud_layer_boundaries.x + offset) * eye_dir;
    march_dist -= length(offset * eye_dir);

    const f32 sun_cos_theta = dot(eye_dir, sun.direction);
    float phase = dual_cloud_phase(sun_cos_theta, clouds.phase_values);

    float3 step_pos_sum = float3(0.0);
    float transmittance_sum = 0.0;
    float depth_sum = 0.0;
    f32 transmittance = 1.0;
    float3 luminance = float3(0.0);

    float old_ray_shift = 0.0;
    const let step_count = float(clouds.clouds_step_count);
    for (int i = 0; i < clouds.clouds_step_count; i++) {
        float new_ray_shift = march_dist * (float(i) + 0.3) / step_count;
        float delta_step = new_ray_shift - old_ray_shift;
        float3 step_pos = new_ray_shift * eye_dir + start_pos;
        float3 step_pos_m = step_pos * INV_CAMERA_SCALE_UNIT;
        old_ray_shift = new_ray_shift;

        float density = sample_density(step_pos_m);
        if (density > DENSITY_EPSILON) {
            if (transmittance < 1.0) {
                float4 clip_step_pos = mul(camera.inv_projection_view_mat, float4(step_pos, 1.0));
                float cloud_z = clip_step_pos.z / clip_step_pos.w;
                depth_sum += transmittance * cloud_z;
                step_pos_sum += step_pos * transmittance;
                transmittance_sum += transmittance;
            }

            float h = length(step_pos);
            float3 up_vec = step_pos / h;
            float step_sun_cos_theta = dot(up_vec, sun.direction);

            float sun_transmittance = sample_sun_density(step_pos);

            float step_optical_depth = density * delta_step * clouds.extinction_coeff;
            float step_transmittance = max(exp(-step_optical_depth), exp(-step_optical_depth * 0.25) * 0.7);
            float powder_transmittance = clouds.powder_intensity * (1.0 - exp(-density * 2.0));

            // Atmos Transmittance
            float2 transmittance_uv = get_transmittance_lut_uv(atmos, float2(h, step_sun_cos_theta));
            float3 atmos_transmittance = transmittance_lut.Sample(linear_clamp_to_edge_sampler, transmittance_uv).xyz;
            
            float3 step_luminance = atmos_transmittance * sun_transmittance * phase * powder_transmittance * clouds.scattering_coeff;

            // frostbite luminance approximation
            float3 integral = (step_luminance - step_luminance * step_transmittance) / clouds.extinction_coeff;
            luminance += sun.intensity * (integral * transmittance);
            transmittance *= step_transmittance;
        }

        if (transmittance < 0.001) {
            break;
        }
    }

    if (transmittance_sum > 0.0) {
        step_pos_sum /= transmittance_sum;
        float3 p = march_dist - float3(0.0, atmos.planet_radius, 0.0);
        float4 v = mul(camera.view_mat, float4(p, 1.0));
        float ray_hit_h = length(step_pos_sum - eye_pos);
        float slice = ray_hit_h * (1.0 / atmos.aerial_gain_per_slice);
        float weight = 1.0;
        if (slice < 0.5) {
            weight = saturate(slice * 2.0);
            slice = 0.5;
        }

        float w = sqrt(slice / float(atmos.aerial_perspective_lut_size.z)); // from linear space to squared distribution
        float4 ap = weight * aerial_perspective_lut.Sample(linear_clamp_to_edge_sampler, float3(input.tex_coord, w));
        ap *= 0.5;
        luminance = luminance * (1.0 - ap.w) + ap.xyz * (1.0 - transmittance);
    }

    return float4(luminance, transmittance);
}
