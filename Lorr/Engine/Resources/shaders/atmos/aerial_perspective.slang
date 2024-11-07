import atmos;
import "../lorr";

[[vk::push_constant]] struct {
    World *world;
    float3 target_image_size;
    ImageView target_image;
    ImageView transmittance_image;
    ImageView ms_image;
} c;

float relative_luminance(float3 c) {
    return 0.2126 * c.r + 0.7152 * c.g + 0.0722 * c.b;
}

[shader("compute")]
[numthreads(16, 16, 1)]
void cs_main(uint3 thread_id : SV_DispatchThreadID) {
    Camera *camera = &c.world->cameras[c.world->active_camera_index];
    Atmosphere *atmos = &c.world->atmosphere;
    Sun *sun = &c.world->sun;

    float2 uv = (float2(thread_id.xy) + 0.5) / c.target_image_size.xy;
    float3 clip_space = float3(2.0 * uv - 1.0, 0.5);
    float4 world_space = mul(camera->inv_projection_view_mat, float4(clip_space, 1.0));
    float3 world_pos = world_space.xyz / world_space.w;
    float3 direction = normalize(world_pos - camera.position);
    float eye_height = camera.position.y * CAMERA_SCALE_UNIT + PLANET_RADIUS_OFFSET;
    vec3 eye_pos = float3(0.0, eye_height + atmos.planet_radius, 0.0);

    float max_dist = 0.0;
    if (!ray_hit_sphere(eye_pos, direction, atmos.planet_radius, max_dist)) {
        ray_hit_sphere(eye_pos, direction, atmos.atmos_radius, max_dist);
    }

    float cos_theta = dot(sun->direction, direction);
    float rayleigh_phase = get_rayleigh_phase(-cos_theta);
    float mie_phase = get_mie_phase(atmos, cos_theta);

    int step_count = int(max(1.0, float(thread_id.z + 1.0) * 2.0));
    float slice = ((float(thread_id.z) + 0.5) * (1.0 / c.target_image_size.z));
    slice *= slice;
    slice *= c.target_image_size.z;

    float t_max = slice * atmos->aerial_km_per_slice;
    float integration_length = min(t_max, max_dist);
    float integration_step = 0.0;
    float old_ray_shift = 0.0;

    float3 luminance = float3(0.0, 0.0, 0.0);
    float3 transmittance = float3(1.0, 1.0, 1.0);

    for (int i = 0; i < step_count; i++) {
        // We use non-linear integration mentioned in paper, high number of steps
        // near camera, low number of steps from far away camera
        float new_ray_shift = integration_length * (float(i) + 0.3) / step_count;
        integration_step = new_ray_shift - old_ray_shift;
        float3 step_pos = eye_pos + new_ray_shift * direction;
        old_ray_shift = new_ray_shift;

        float h = length(step_pos);
        float altitude = h - atmos->planet_radius;

        // Shadowing factor
        float3 up_vec = normalize(step_pos);
        f32 sun_theta = dot(sun->direction, up_vec);

        float2 transmittance_uv = get_transmittance_lut_uv(atmos, float2(altitude, sun_theta));
        float3 sun_transmittance = c.transmittance_image.sample(c.world->linear_sampler, transmittance_uv).rgb;

        float2 multiscatter_uv = get_multiscattering_lut_uv(atmos, altitude, sun_theta);
        float3 MS = c.ms_image.sample(c.world->linear_sampler, multiscatter_uv).rgb;

        // Get scattering coefficient
        MediumScattering medium_info = MediumScattering(atmos, altitude);
        float3 scattering_phase = medium_info.mie_scattering * mie_phase + medium_info.rayleigh_scattering * rayleigh_phase;
        float earth_shadow =
            ray_sphere_intersect_nearest(step_pos, sun->direction, PLANET_RADIUS_OFFSET * up_vec, atmos->planet_radius) == -1.0 ? 1.0 : 0.0;
        float3 sun_luminance = earth_shadow * sun_transmittance * scattering_phase + MS * (medium_info.rayleigh_scattering + medium_info.mie_scattering);

        float3 step_transmittance = exp(-integration_step * medium_info.extinction_sum);
        float3 integral = (sun_luminance - sun_luminance * step_transmittance) / medium_info.extinction_sum;

        luminance += integral * transmittance;
        transmittance *= step_transmittance;
    }

    c.target_image.store(thread_id, float4(luminance, 1.0 - relative_luminance(transmittance)));
}
