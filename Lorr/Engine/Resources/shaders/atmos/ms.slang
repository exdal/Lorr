import "../lorr";
import "common";

[[vk::push_constant]] struct {
    lr::World *world;
    BufferID sample_buffer_id;
    ImageViewID transmittance_image_id;
    SamplerID transmittance_sampler_id;
    float2 image_size;
    ImageViewID target_image;
} c;

float3 to_sample_dir(float2 sample_pos) {
    float dx = 1.0 - 2.0 * sample_pos.x;
    float radius = sqrt(max(0, 1.0 - dx * dx));
    float phi = 2.0 * PI * sample_pos.y;
    return float3(float2(cos(phi), sin(phi)) * radius, dx);
}

float3 integrate(lr::Atmosphere *atmos, float3 ray_pos, float3 sun_dir) {
    const float step_count = 48.0;
    float2 *samples = lr::load_buffer<float2>(c.sample_buffer_id);

    float3 L2_sum = float3(0.0);
    float3 Fms_sum = float3(0.0);

    for (uint si = 0; si < 4096; si++) {
        float3 sample_dir = to_sample_dir(samples[si]);
        float3 L2 = float3(0.0);
        float3 Fms = float3(0.0);
        float3 transmittance = float3(1.0);

        float max_dist = 0.0;
        bool planet_hit = ray_hit_sphere(ray_pos, sample_dir, atmos.planet_radius, max_dist);
        if (!planet_hit) {
            ray_hit_sphere(ray_pos, sample_dir, atmos.atmos_radius, max_dist);
        }

        float cos_theta = dot(sample_dir, sun_dir);
        float rayleigh_phase = get_rayleigh_phase(-cos_theta);
        float mie_phase = get_mie_phase(atmos, cos_theta);

        float step_size = max_dist / step_count;
        float t = 0.0;
        for (float i = 0.0; i < step_count; i += 1.0) {
            float next_step = step_size * i;
            float deltaT = next_step - t;
            t = next_step;

            float3 step_position = ray_pos + t * sample_dir;
            float h = length(step_position);
            float altitude = h - atmos.planet_radius;
            float3 extinction = extinction_sum(atmos, altitude);
            float3 altitude_trans = exp(-deltaT * extinction);
            float sun_theta = dot(sun_dir, step_position / h);
            float3 sun_trans = sample_lut(atmos, c.transmittance_image_id, c.transmittance_sampler_id, altitude, sun_theta);

            float3 rayleigh_coeff;
            float mie_coeff;
            get_scattering(atmos, altitude, rayleigh_coeff, mie_coeff);

            float3 rayleigh_inner_s = rayleigh_coeff * rayleigh_phase;
            float3 mie_inner_s = mie_coeff * mie_phase;
            float3 scattering_phase = (rayleigh_inner_s + mie_inner_s) * sun_trans;

            // (6)
            float3 integral_lum = (scattering_phase - scattering_phase * altitude_trans) / extinction;
            L2 += integral_lum * transmittance;

            // (7)
            float3 scattering_no_phase = rayleigh_coeff + mie_coeff;
            float3 integral_fms = (scattering_no_phase - scattering_no_phase * altitude_trans) / extinction;
            Fms += integral_fms * transmittance;
            transmittance *= altitude_trans;
        }

        if (planet_hit) {
            const float3 terrain_albedo = float3(0.3);
            float3 hit_pos = ray_pos + max_dist * sample_dir;
            float h = length(hit_pos);
            float3 up = hit_pos / h;
            float altitude = h - atmos.planet_radius;
            float theta = dot(sun_dir, up);
            float3 sun_trans = sample_lut(atmos, c.transmittance_image_id, c.transmittance_sampler_id, altitude, theta);
            float light_theta = saturate(dot(up, sun_dir));
            L2 += sun_trans * transmittance * light_theta * terrain_albedo / PI;
        }

        L2_sum += L2;
        Fms_sum = Fms;
    }

    float3 L2 = L2_sum / 4096.0;
    float3 Fms = Fms_sum / 4096.0;

    return L2 / (1.0 - Fms);
}

[shader("compute")]
[numthreads(16, 16, 1)]
void cs_main(uint3 thread_id : SV_DispatchThreadID) {
    lr::Atmosphere *atmos = &c.world->atmosphere;
    let target_image = RWTexture2D<float4>::get(c.target_image);
    float2 uv = float2(thread_id.xy) / c.image_size;

    float h = lerp(atmos->planet_radius, atmos->atmos_radius, uv.y);
    float3 ray_pos = float3(0.0, h, 0.0);
    float sun_cos_theta = 2.0 * uv.x - 1.0;
    float3 sun_dir = float3(0.0, sun_cos_theta, sqrt(1.0 - sun_cos_theta * sun_cos_theta));

    target_image[thread_id.xy] = float4(integrate(atmos, ray_pos, sun_dir), 1.0);
}
