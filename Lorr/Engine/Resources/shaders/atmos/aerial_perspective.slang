import atmos;
import "../lorr";

[[vk::push_constant]] struct {
    World *world;
    float3 target_image_size;
    ImageViewID target_image;
    ImageViewID transmittance_image;
    float z_far;
    u32 pad;
} c;

float relative_luminance(float3 c) {
    return 0.2126 * c.r + 0.7152 * c.g + 0.0722 * c.b;
}

[shader("compute")]
[numthreads(16, 16, 1)]
void cs_main(uint3 thread_id : SV_DispatchThreadID) {
    const let target_image = ImageView(c.target_image);
    const let transmittance_image = ImageView(c.transmittance_image);
    Camera *camera = &c.world->cameras[c.world->active_camera_index];
    Atmosphere *atmos = &c.world->atmosphere;
    Sun *sun = &c.world->sun;

    float2 uv = float2(thread_id.x + 0.5, thread_id.y + 0.5) / c.target_image_size.xy;
    float4 clip_space = float4(2.0 * uv - 1.0, 1.0, 1.0);
    float4 world_space = mul(camera->inv_projection_view_mat, clip_space);
    float3 world_pos = world_space.xyz / world_space.w;

    float3 eye_pos = float3(0.0, atmos->planet_radius + camera->position.y + 2.0, 0.0);
    float3 direction = normalize(world_pos);
    float cos_theta = dot(direction, sun->direction);

    float max_dist = 0.0;
    if (!ray_hit_sphere(eye_pos, direction, atmos.planet_radius, max_dist)) {
        ray_hit_sphere(eye_pos, direction, atmos.atmos_radius, max_dist);
    }

    float rayleigh_phase = get_rayleigh_phase(-cos_theta);
    float mie_phase = get_mie_phase(atmos, cos_theta);

    float3 luminance = float3(0.0, 0.0, 0.0);
    float3 transmittance = float3(1.0, 1.0, 1.0);

    const float step_count = 8.0;
    float depth = c.target_image_size.z;
    float slice_depth = c.z_far / depth;

    float t_begin = 0;
    float t_end = min(0.5 * slice_depth, max_dist);
    for (float z = 0.0; z < depth; z += 1.0) {
        float step_size = (t_end - t_begin) / step_count;
        float t = 0.0;

        for (float i = 0.0; i < step_count; i += 1.0) {
            float next_t = step_size * i;
            float delta_t = next_t - t;
            t = next_t;

            float3 step_pos = eye_pos + t * direction;
            float h = length(step_pos);
            float altitude = h - atmos->planet_radius;
            float3 extinction = extinction_sum(atmos, altitude);

            // Shadowing factor
            float3 up_vec = normalize(step_pos);
            f32 sun_theta = dot(sun->direction, up_vec);
            float2 transmittance_uv = get_transmittance_lut_uv(atmos, float2(h, sun_theta));
            float3 sun_transmittance = transmittance_image.sample(c.world->linear_sampler, transmittance_uv).rgb;

            // Get scattering coefficient
            float3 rayleigh_scattering;
            float mie_scattering;
            get_scattering(*atmos, altitude, rayleigh_scattering, mie_scattering);

            float3 scattering_phase = mie_scattering * mie_phase + rayleigh_scattering * rayleigh_phase;
            float _ = 0;
            float earth_shadow = !ray_hit_sphere(step_pos, sun->direction, atmos->planet_radius, _) ? 1.0 : 0.0;
            float3 sun_luminance = earth_shadow * sun_transmittance * scattering_phase;
            float3 step_transmittance = exp(-delta_t * extinction);

            float3 integral = (sun_luminance - sun_luminance * step_transmittance) / extinction;

            luminance += sun->intensity * (integral * transmittance);
            transmittance *= step_transmittance;
        }

        target_image.rw_3d()[uint3(thread_id.xy, uint(z))] = float4(luminance, relative_luminance(transmittance));

        t_begin = t_end;
        t_end = min(t_end + slice_depth, max_dist);
    }
}
