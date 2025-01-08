module cloud;

import "../lorr";
import "../atmos/atmos";

public float remap(float orig, float old_min, float old_max, float new_min, float new_max) {
    return new_min + (((orig - old_min) / (old_max - old_min)) * (new_max - new_min));
}

// WARN: These hashes are extremely fragile. Replacing one of them would collapse all noise functions.
public float hash13(float3 p) {
    const static uint3 VPRIME = uint3(3480082861U, 2420690917U, 2149110343U);
    const static uint FPRIME = 1317666547U;
    const static float UIF = 1.0 / float(0xffffffffU);

    uint3 q = uint3(int3(p)) * VPRIME;
    uint n = (q.x & q.y ^ q.z) * FPRIME;
    return float(n) * UIF;
}

public float3 hash33(float3 p) {
    const static uint UI0 = 1597334673U;
    const static uint UI1 = 3812015801U;
    const static uint2 UI2 = uint2(UI0, UI1);
    const static uint3 UI3 = uint3(UI0, UI1, 2798796415U);
    const static float UIF = 1.0 / float(0xffffffffU);

    uint3 q = uint3(int3(p)) * UI3;
    q = (q.x ^ q.y ^ q.z) * UI3;
    return float3(q) * UIF;
}


float3 get_cloud_layer_boundaries(World *world, float3 eye_pos, float3 eye_dir) {
    float planet_radius = world->atmosphere.planet_radius;
    float2 cloud_layer_boundaries = world->clouds.bounds + float2(planet_radius);
    float eye_altitude = length(eye_pos);

    if (eye_altitude < cloud_layer_boundaries.x) {
        // Camera is under cloud layer
        float t_min = lr::ray_sphere_intersect_inside(eye_pos, eye_dir, cloud_layer_boundaries.x);
        float t_max = lr::ray_sphere_intersect_inside(eye_pos, eye_dir, cloud_layer_boundaries.y);
        float t_earth = lr::ray_sphere_intersect_nearest(eye_pos, eye_dir, planet_radius).hasValue ? 1.0 : 0.0;

        return float3(t_min, t_max, t_earth);
    } else if (eye_altitude > cloud_layer_boundaries.y) {
        // Camera is above cloud layer
        const let tv0 = lr::ray_sphere_intersect_outside(eye_pos, eye_dir, cloud_layer_boundaries.y);
        const let tv1 = lr::ray_sphere_intersect_outside(eye_pos, eye_dir, cloud_layer_boundaries.x);
        
        if (tv1.hasValue) {
            return float3(tv0.value.x, tv1.value.x, tv0.hasValue ? 0.0 : 1.0);
        } else {
            return float3(tv0.value.x, tv1.value.y, tv0.hasValue ? 0.0 : 1.0);
        }
    } else {
        // Camera is inside cloud layer
        const let x = lr::ray_sphere_intersect_nearest(eye_pos, eye_dir, cloud_layer_boundaries.x);
        if (x.hasValue) {
            return float3(0.0, x.value, 0.0);
        } else {
            return float3(0.0, lr::ray_sphere_intersect_inside(eye_pos, eye_dir, cloud_layer_boundaries.y), 0.0);
        }
    }
}

func sample_density(
    World *world,
    float3 pos_m,
    in SampledImage<float4> cloud_shape_image,
    in SampledImage<float4> cloud_detail_image) -> float
{
    const float cloud_layer_min_km = world->clouds.bounds.x;
    const float cloud_layer_max_km = world->clouds.bounds.y;
    const float cloud_layer_thickness_km = cloud_layer_max_km - cloud_layer_min_km;

    float step_pos_height = length(pos_m * CAMERA_SCALE_UNIT);
    float height_gradient = ((step_pos_height - world->atmosphere.planet_radius) - cloud_layer_min_km) / cloud_layer_thickness_km;

    pos_m += height_gradient;
    float3 pos_km = pos_m * CAMERA_SCALE_UNIT;
    pos_km *= world->clouds.global_scale;

    float4 shape_noise = cloud_shape_image.sample(pos_km);
    float shape_noise_fbm = dot(shape_noise.yzw, normalize(world->clouds.shape_noise_weights));
    float base_cloud = shape_noise_fbm - min(world->clouds.shape_noise_scale, 1.0);

    float coverage = world->clouds.coverage;
    base_cloud = remap(base_cloud, 1.0 - coverage, 1.0, 0.0, 1.0);
    base_cloud *= coverage;

    float vertical_density = cloud_detail_image.sample(float3(world->clouds.cloud_type, height_gradient, pos_km.z)).w;
    base_cloud *= vertical_density;

    if (base_cloud > 0.0) {
        float4 detail_noise = cloud_detail_image.sample(pos_km * world->clouds.detail_noise_scale);
        float detail_noise_fbm = dot(detail_noise.xyz, normalize(world->clouds.detail_noise_weights));
        float one_minus_shape = 1.0 - base_cloud;
        float detail_erode_weight = one_minus_shape * one_minus_shape * one_minus_shape;
        float final_cloud = base_cloud - (1.0 - detail_noise_fbm) * detail_erode_weight * world->clouds.detail_noise_influence;
        return final_cloud * world->clouds.general_density * height_gradient;
    }

    return 0.0;
}

func sample_sun_density(
    World *world,
    float3 pos_km,
    float3 sun_dir,
    in SampledImage<float4> cloud_shape_image,
    in SampledImage<float4> cloud_detail_image) -> float
{
    float3 cloud_layer_boundaries = get_cloud_layer_boundaries(world, pos_km, sun_dir);
    cloud_layer_boundaries.xy = max(cloud_layer_boundaries.xy, 0.0);
    float march_dist = cloud_layer_boundaries.y - cloud_layer_boundaries.x;
    cloud_layer_boundaries.y = cloud_layer_boundaries.x + march_dist;
    float step_count = float(world->clouds.sun_step_count);
    float step_t = (cloud_layer_boundaries.y - cloud_layer_boundaries.x) / step_count;
    float sample_t = cloud_layer_boundaries.x + 0.001 * step_t;

    // I have no idea why this has some weird shadowing effects,
    // maybe the ray intersects with neighboring cloud?
    float total_density = 0.0;
    for (int i = 0; i < step_count; i++) {
        float3 step_pos = sample_t * sun_dir + pos_km;
        float3 step_pos_m = step_pos * (1.0 / CAMERA_SCALE_UNIT);
        float density = sample_density(world, step_pos_m, cloud_shape_image, cloud_detail_image);
        total_density += max(density, 0.0);
        sample_t += step_t;
    }

    // Do we have to convert this to meters?
    float transmittance = exp(-total_density * (1.0 / CAMERA_SCALE_UNIT) * world->clouds.sun_light_absorption);
    return world->clouds.darkness_threshold + transmittance * (1.0 - world->clouds.darkness_threshold);
}

float dual_cloud_phase(float cos_theta, float3 params) {
    float forwards_lobe =  lr::henyey_greenstein_phase( params.x, cos_theta);
    float backwards_lobe = lr::henyey_greenstein_phase(-params.y, cos_theta);
    float mix_factor =     lr::henyey_greenstein_phase( params.z, cos_theta);
    return lerp(lerp(forwards_lobe, backwards_lobe, 0.2), mix_factor, 0.15);
}

public struct CloudLuminance {
    public float3 luminance = 0.0;
    public float transmittance = 1.0;
    public float depth = 1.0;
};

public struct CloudIntegrateInfo {
    public Camera *camera = nullptr;
    public float2 uv = 0.0;
    public float sample_offset = 0.0;
    public float3 eye_pos = 0.0;
    public float3 eye_dir = 0.0;
    public SampledImage<float4> cloud_shape_image;
    public SampledImage<float4> cloud_detail_image;
    public SampledImage<float4> transmittance_lut_image;
    public SampledImage<float4> aerial_perspective_lut_image;
};

public func integrate_clouds(World *world, in CloudIntegrateInfo info) -> CloudLuminance {
    Clouds *clouds = world->clouds;
    Atmosphere *atmos = world->atmosphere;

    float draw_distance = clouds->draw_distance * CAMERA_SCALE_UNIT;
    float3 cloud_layer_boundaries = get_cloud_layer_boundaries(world, info.eye_pos, info.eye_dir);
    cloud_layer_boundaries.xy = max(cloud_layer_boundaries.xy, 0.0);
    if (cloud_layer_boundaries.z == 1.0 ||
        cloud_layer_boundaries.y <= cloud_layer_boundaries.x ||
        cloud_layer_boundaries.x > draw_distance) {
        return {};
    }

    float3 sun_dir = world->sun.direction;
    float march_dist = min(cloud_layer_boundaries.y - cloud_layer_boundaries.x, draw_distance);
    cloud_layer_boundaries.y = cloud_layer_boundaries.x + march_dist;
    float step_count = float(clouds->clouds_step_count);
    float step_t = (cloud_layer_boundaries.y - cloud_layer_boundaries.x) / step_count;
    float sample_t = cloud_layer_boundaries.x + CAMERA_SCALE_UNIT * step_t;
    sample_t += step_t * info.sample_offset;

    float sun_cos_theta = dot(info.eye_dir, sun_dir);

    float phase = dual_cloud_phase(-sun_cos_theta, clouds->phase_values);

    // dual atmos transmittance
    float3 atmos_transmittance_0 = 0.0;
    {
        float3 sample_pos = sample_t * info.eye_dir + info.eye_pos;
        float h = length(sample_pos);
        float3 up_vec = sample_pos / h;
        float transmittance_sun_cos_theta = dot(up_vec, sun_dir);
        float2 transmittance_uv = get_transmittance_lut_uv(atmos, float2(h, transmittance_sun_cos_theta));
        atmos_transmittance_0 = info.transmittance_lut_image.sample(transmittance_uv).rgb;
    }
    float3 atmos_transmittance_1 = 0.0;
    {
        float3 sample_pos = cloud_layer_boundaries.y * info.eye_dir + info.eye_pos;
        float h = length(sample_pos);
        float3 up_vec = sample_pos / h;
        float transmittance_sun_cos_theta = dot(up_vec, sun_dir);
        float2 transmittance_uv = get_transmittance_lut_uv(atmos, float2(h, transmittance_sun_cos_theta));
        atmos_transmittance_1 = info.transmittance_lut_image.sample(transmittance_uv).rgb;
    }

    float linear_depth_sum = 0.0;
    float transmittance_sum = 0.0;

    float3 ray_hit_pos = 0.0;
    float ray_hit_pos_weight = 0.0;

    float transmittance = 1.0;
    float3 luminance = float3(0.0);
    for (int i = 0; i < step_count; i++) {
        float3 step_pos = sample_t * info.eye_dir + info.eye_pos;
        ray_hit_pos += step_pos * transmittance;
        ray_hit_pos_weight += transmittance;

        // sample_t gets increased each step no matter the density, keep it outside if statement below
        float3 atmos_transmittance = lerp(atmos_transmittance_0, atmos_transmittance_1, saturate(sample_t / march_dist));

        float3 step_pos_m = step_pos * (1.0 / CAMERA_SCALE_UNIT);
        float density = sample_density(world, step_pos_m, info.cloud_shape_image, info.cloud_detail_image);
        if (density > 0.0) {
            if (transmittance < 1.0) {
                float4 ppos = mul(info.camera->projection_view_mat, float4(step_pos, 1.0));
                float cloud_z = ppos.z / ppos.w;
                linear_depth_sum += transmittance * cloud_z;
                transmittance_sum += transmittance;
            }

            float sun_density = sample_sun_density(world, step_pos, sun_dir, info.cloud_shape_image, info.cloud_detail_image);
            float optical_depth = density * step_t * (1.0 / CAMERA_SCALE_UNIT);
            float step_transmittance = exp(-optical_depth * world->clouds.cloud_light_absorption);

            float powder =  8.0 * (1.0 - 0.97 * exp(-2.0 * density));
            float powder_transmittance = lerp(powder, 1.0, sun_cos_theta * 0.5 + 0.5);

            float3 step_luminance = sun_density * density * phase;
            float3 integral = (step_luminance - step_luminance * step_transmittance * powder_transmittance) / density;

            luminance += world->sun.intensity * (integral * transmittance * atmos_transmittance);
            transmittance *= step_transmittance;
        }
        if(transmittance < 0.001) {
            break;
        }

        sample_t += step_t;
    }

    if (false && ray_hit_pos_weight > 0.0) {
        ray_hit_pos /= ray_hit_pos_weight;
        float3 p = ray_hit_pos - float3(0.0, atmos->planet_radius, 0.0);
        float4 v = mul(info.camera->view_mat, float4(p, 1.0));
        ray_hit_pos -= info.eye_pos;
        float ray_hit_h = length(ray_hit_pos);

        float slice = ray_hit_h * (1.0 / atmos->aerial_km_per_slice);
        float weight = 1.0;
        if (slice < 0.5) {
            weight = saturate(slice * 2.0);
            slice = 0.5;
        }

        float w = sqrt(slice / 32.0); // from linear space to squared distribution
        float4 ap = weight * info.aerial_perspective_lut_image.sample(float3(info.uv, w));
        ap *= 0.5;
        luminance = luminance * (1.0 - ap.a) + ap.rgb * (1.0 - transmittance);
    }

    CloudLuminance result;
    result.luminance = luminance;
    result.transmittance = transmittance;
    result.depth = linear_depth_sum / transmittance_sum;

    return result;
}
