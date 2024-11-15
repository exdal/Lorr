import cloud;
import "../atmos/atmos";
import "../lorr";

[[vk::push_constant]] struct {
    World *world;
    //ImageView<float4> depth_image;
    ImageView<float4> transmittance_lut_image;
    ImageView<float4> aerial_perspective_lut_image;
    SampledImage<float4> shape_noise_image;
    SampledImage<float4> detail_noise_image;
} c;

struct VertexOutput {
    vec4 position     : SV_Position;
    vec2 tex_coord    : TEXCOORD;
};

[shader("vertex")]
VertexOutput vs_main(u32 vertex_id : SV_VertexID) {
    VertexOutput output;

    output.tex_coord = vec2((vertex_id << 1) & 2, vertex_id & 2);
    output.position = vec4(output.tex_coord * 2.0 - 1.0, 0.5, 1);

    return output;
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

float sample_density(World *world, float3 pos_m) {
    const float cloud_layer_min_km = world->clouds.bounds.x;
    const float cloud_layer_max_km = world->clouds.bounds.y;
    const float cloud_layer_thickness_km = cloud_layer_max_km - cloud_layer_min_km;

    float step_pos_height = length(pos_m * CAMERA_SCALE_UNIT);
    float height_gradient = ((step_pos_height - world->atmosphere.planet_radius) - cloud_layer_min_km) / cloud_layer_thickness_km;

    pos_m += height_gradient;
    float3 pos_km = pos_m * CAMERA_SCALE_UNIT;
    pos_km *= world->clouds.global_scale;

    float4 shape_noise = c.shape_noise_image.sample(pos_km);
    float shape_noise_fbm = dot(shape_noise.yzw, normalize(world->clouds.shape_noise_weights));
    float base_cloud = shape_noise_fbm - min(world->clouds.shape_noise_scale, 1.0);

    float coverage = world->clouds.coverage;
    base_cloud = remap(base_cloud, 1.0 - coverage, 1.0, 0.0, 1.0);
    base_cloud *= coverage;

    float vertical_density = c.detail_noise_image.sample(float3(world->clouds.cloud_type, height_gradient, pos_km.z)).w;
    base_cloud *= vertical_density;

    if (base_cloud > 0.0) {
        float4 detail_noise = c.detail_noise_image.sample(pos_km * world->clouds.detail_noise_scale);
        float detail_noise_fbm = dot(detail_noise.xyz, normalize(world->clouds.detail_noise_weights));
        float one_minus_shape = 1.0 - base_cloud;
        float detail_erode_weight = one_minus_shape * one_minus_shape * one_minus_shape;
        float final_cloud = base_cloud - (1.0 - detail_noise_fbm) * detail_erode_weight * world->clouds.detail_noise_influence;
        return final_cloud * world->clouds.general_density * height_gradient;
    }

    return 0.0;
}

float sample_sun_density(World *world, float3 pos_km, float3 sun_dir) {
    float3 cloud_layer_boundaries = get_cloud_layer_boundaries(world, pos_km, sun_dir);
    cloud_layer_boundaries.xy = max(cloud_layer_boundaries.xy, 0.0);
    float march_dist = cloud_layer_boundaries.y - cloud_layer_boundaries.x;
    cloud_layer_boundaries.y = cloud_layer_boundaries.x + march_dist;
    float step_count = float(world->clouds.sun_step_count);
    float step_t = (cloud_layer_boundaries.y - cloud_layer_boundaries.x) / step_count;
    float sample_t = cloud_layer_boundaries.x + 0.001 * step_t;

    float total_density = 1.0;
    for (int i = 0; i < step_count; i++) {
        float3 step_pos = sample_t * sun_dir + pos_km;
        float3 step_pos_m = step_pos * (1.0 / CAMERA_SCALE_UNIT);
        float density = sample_density(world, step_pos_m);
        float optical_depth = density * step_t * (1.0 / CAMERA_SCALE_UNIT);
        total_density += max(optical_depth, 0.0);
    }

    float transmittance = exp(-total_density * 0.245);
    return world->clouds.darkness_threshold + transmittance * (1.0 - world->clouds.darkness_threshold);
}

[shader("fragment")]
float4 fs_main(VertexOutput input) : SV_TARGET {
    World *world = c.world;
    Camera *camera = &world->cameras[world->active_camera_index];
    Clouds *clouds = &world->clouds;
    Atmosphere *atmos = &world->atmosphere;

    float3 clip_space = float3(input.tex_coord * 2.0 - 1.0, 1.0);
    float4 clip_pos = mul(camera->inv_projection_view_mat, float4(clip_space, 1.0));
    float3 clip_dir = clip_pos.xyz / clip_pos.w;
    float3 world_dir = normalize(clip_dir);
    float3 world_pos = camera->position * CAMERA_SCALE_UNIT + float3(0.0, atmos->planet_radius, 0.0);

    float3 cloud_layer_boundaries = get_cloud_layer_boundaries(world, world_pos, world_dir);
    cloud_layer_boundaries.xy = max(cloud_layer_boundaries.xy, 0.0);
    if (cloud_layer_boundaries.z == 1.0 ||
        cloud_layer_boundaries.y <= cloud_layer_boundaries.x ||
        cloud_layer_boundaries.x > (clouds->draw_distance * CAMERA_SCALE_UNIT)) {
        return float4(0.0, 0.0, 0.0, 1.0);
    }

    float3 sun_dir = normalize(world->sun.direction);
    float march_dist = cloud_layer_boundaries.y - cloud_layer_boundaries.x;
    cloud_layer_boundaries.y = cloud_layer_boundaries.x + march_dist;
    float step_count = float(clouds->clouds_step_count);
    float step_t = (cloud_layer_boundaries.y - cloud_layer_boundaries.x) / step_count;
    float sample_t = cloud_layer_boundaries.x + 0.001 * step_t;

    float sun_cos_theta = dot(world_dir, sun_dir);
    float phase = lr::dual_henyey_greenstein_phase(clouds->phase_values.x, clouds->phase_values.y, clouds->phase_values.z, -sun_cos_theta);
    // dual atmos transmittance
    float3 atmos_transmittance_0 = 0.0;
    {
        float3 sample_pos = sample_t * world_dir + world_pos;
        float h = length(sample_pos);
        float3 up_vec = sample_pos / h;
        float transmittance_sun_cos_theta = dot(up_vec, sun_dir);
        float2 transmittance_uv = get_transmittance_lut_uv(atmos, float2(h, transmittance_sun_cos_theta));
        atmos_transmittance_0 = c.transmittance_lut_image.sample(world->linear_sampler, transmittance_uv).rgb;
    }
    float3 atmos_transmittance_1 = 0.0;
    {
        float3 sample_pos = cloud_layer_boundaries.y * world_dir + world_pos;
        float h = length(sample_pos);
        float3 up_vec = sample_pos / h;
        float transmittance_sun_cos_theta = dot(up_vec, sun_dir);
        float2 transmittance_uv = get_transmittance_lut_uv(atmos, float2(h, transmittance_sun_cos_theta));
        atmos_transmittance_1 = c.transmittance_lut_image.sample(world->linear_sampler, transmittance_uv).rgb;
    }

    float linear_depth_sum = 0.0;
    float transmittance_sum = 0.0;

    float3 ray_hit_pos = 0.0;
    float ray_hit_pos_weight = 0.0;

    float transmittance = 1.0;
    float3 luminance = float3(0.0);
    for (int i = 0; i < step_count; i++) {
        float3 step_pos = sample_t * world_dir + world_pos;
        ray_hit_pos += step_pos * transmittance;
        ray_hit_pos_weight += transmittance;

        // sample_t gets increased each step no matter the density, keep it outside if statement below
        float3 atmos_transmittance = lerp(atmos_transmittance_0, atmos_transmittance_1, saturate(sample_t / march_dist));

        float3 step_pos_m = step_pos * (1.0 / CAMERA_SCALE_UNIT);
        float density = sample_density(world, step_pos_m);
        if (density > 0.0) {
            if (transmittance < 1.0) {
                float4 ppos = mul(camera->projection_view_mat, float4(step_pos, 1.0));
                float cloud_z = ppos.z / ppos.w;
                linear_depth_sum += transmittance * cloud_z;
                transmittance_sum += transmittance;
            }
            float sun_density = sample_sun_density(world, step_pos, sun_dir);
            float optical_depth = density * step_t * (1.0 / CAMERA_SCALE_UNIT);
            float step_transmittance = exp(-optical_depth * 0.446);
            float powder_transmittance = exp(-optical_depth * 0.446 * 2.0);
            float sun_light = sun_density * phase * density;

            float integral = (sun_light - sun_light * step_transmittance * powder_transmittance) / density;
            luminance += world->sun.intensity * (integral * transmittance * atmos_transmittance);
            transmittance *= step_transmittance * powder_transmittance;
        }
        if(transmittance < 0.001) {
            break;
        }

        sample_t += step_t;
    }

    if (ray_hit_pos_weight > 0.0) {
        ray_hit_pos /= ray_hit_pos_weight;
        float3 p = ray_hit_pos - float3(0.0, atmos->planet_radius, 0.0);
        float4 v = mul(camera->view_mat, float4(p, 1.0));
        ray_hit_pos -= world_pos;
        float ray_hit_h = length(ray_hit_pos);

        float slice = ray_hit_h * (1.0 / atmos->aerial_km_per_slice);
        float weight = 1.0;
        if (slice < 0.5) {
            weight = saturate(slice * 2.0);
            slice = 0.5;
        }

        float w = sqrt(slice / 32.0); // from linear space to squared distribution
        float4 ap = weight * c.aerial_perspective_lut_image.sample(world->linear_sampler, float3(input.tex_coord, w));
        ap *= 0.5;
        luminance = luminance * (1.0 - ap.a) + ap.rgb * (1.0 - transmittance);
    }

    // float cloud_depth = linear_depth_sum / transmittance_sum;
    return float4(luminance, transmittance);
}
