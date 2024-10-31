import atmos;
import "../lorr";

[[vk::push_constant]] struct {
    World *world;
    ImageViewID transmittance_image_id;
    float2 ms_image_size;
    ImageViewID ms_image_id;
    u32 pad;
} c;

const static vec3 Samples[64] = {
    vec3(-0.7838,-0.620933,0.00996137),
    vec3(0.106751,0.965982,0.235549),
    vec3(-0.215177,-0.687115,-0.693954),
    vec3(0.318002,0.0640084,-0.945927),
    vec3(0.357396,0.555673,0.750664),
    vec3(0.866397,-0.19756,0.458613),
    vec3(0.130216,0.232736,-0.963783),
    vec3(-0.00174431,0.376657,0.926351),
    vec3(0.663478,0.704806,-0.251089),
    vec3(0.0327851,0.110534,-0.993331),
    vec3(0.0561973,0.0234288,0.998145),
    vec3(0.0905264,-0.169771,0.981317),
    vec3(0.26694,0.95222,-0.148393),
    vec3(-0.812874,-0.559051,-0.163393),
    vec3(-0.323378,-0.25855,-0.910263),
    vec3(-0.1333,0.591356,-0.795317),
    vec3(0.480876,0.408711,0.775702),
    vec3(-0.332263,-0.533895,-0.777533),
    vec3(-0.0392473,-0.704457,-0.708661),
    vec3(0.427015,0.239811,0.871865),
    vec3(-0.416624,-0.563856,0.713085),
    vec3(0.12793,0.334479,-0.933679),
    vec3(-0.0343373,-0.160593,-0.986423),
    vec3(0.580614,0.0692947,0.811225),
    vec3(-0.459187,0.43944,0.772036),
    vec3(0.215474,-0.539436,-0.81399),
    vec3(-0.378969,-0.31988,-0.868366),
    vec3(-0.279978,-0.0109692,0.959944),
    vec3(0.692547,0.690058,0.210234),
    vec3(0.53227,-0.123044,-0.837585),
    vec3(-0.772313,-0.283334,-0.568555),
    vec3(-0.0311218,0.995988,-0.0838977),
    vec3(-0.366931,-0.276531,-0.888196),
    vec3(0.488778,0.367878,-0.791051),
    vec3(-0.885561,-0.453445,0.100842),
    vec3(0.71656,0.443635,0.538265),
    vec3(0.645383,-0.152576,-0.748466),
    vec3(-0.171259,0.91907,0.354939),
    vec3(-0.0031122,0.9457,0.325026),
    vec3(0.731503,0.623089,-0.276881),
    vec3(-0.91466,0.186904,0.358419),
    vec3(0.15595,0.828193,-0.538309),
    vec3(0.175396,0.584732,0.792038),
    vec3(-0.0838381,-0.943461,0.320707),
    vec3(0.305876,0.727604,0.614029),
    vec3(0.754642,-0.197903,-0.62558),
    vec3(0.217255,-0.0177771,-0.975953),
    vec3(0.140412,-0.844826,0.516287),
    vec3(-0.549042,0.574859,-0.606705),
    vec3(0.570057,0.17459,0.802841),
    vec3(-0.0330304,0.775077,0.631003),
    vec3(-0.938091,0.138937,0.317304),
    vec3(0.483197,-0.726405,-0.48873),
    vec3(0.485263,0.52926,0.695991),
    vec3(0.224189,0.742282,-0.631472),
    vec3(-0.322429,0.662214,-0.676396),
    vec3(0.625577,-0.12711,0.769738),
    vec3(-0.714032,-0.584461,-0.385439),
    vec3(-0.0652053,-0.892579,-0.446151),
    vec3(0.408421,-0.912487,0.0236566),
    vec3(0.0900381,0.319983,0.943135),
    vec3(-0.708553,0.483646,0.513847),
    vec3(0.803855,-0.0902273,0.587942),
    vec3(-0.0555802,-0.374602,-0.925519),
};

vec3 integrate(Atmosphere *atmos, vec3 ray_pos, vec3 sun_dir) {
    const float step_count = 256.0;
    const uint sample_count = 64;

    vec3 L2_sum = vec3(0.0);
    vec3 Fms_sum = vec3(0.0);

    for (uint si = 0; si < sample_count; si++) {
        vec3 sample_dir = Samples[si];
        vec3 L2 = vec3(0.0);
        vec3 Fms = vec3(0.0);
        vec3 transmittance = vec3(1.0);

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
            float next_t = i * step_size;
            float dt = next_t - t;
            t = next_t;

            vec3 step_position = ray_pos + t * sample_dir;
            float h = length(step_position);
            float altitude = h - atmos.planet_radius;
            vec3 up = step_position / h;

            vec3 rayleigh_scatter = atmos->rayleigh_scatter * exp(-altitude / atmos->rayleigh_density);
            float mie_scatter = (atmos->mie_scatter + atmos->mie_absorption) * exp(-altitude / atmos->mie_density);

            vec3 extinction = extinction_sum(atmos, altitude);
            vec3 altitude_trans = exp(-dt * extinction);
            float sun_theta = dot(sun_dir, up);
            vec3 sun_trans = sample_lut(atmos, c.transmittance_image_id, c.world.linear_sampler, altitude, sun_theta);

            // (6)
            vec3 rayleigh_inner_s = rayleigh_scatter * rayleigh_phase;
            vec3 mie_inner_s = mie_scatter * mie_phase;
            vec3 scattering_phase = (rayleigh_inner_s + mie_inner_s) * sun_trans;
            vec3 integral_lum = (scattering_phase - scattering_phase * altitude_trans) / extinction;
            L2 += integral_lum * transmittance;

            // (7)
            vec3 scattering_no_phase = rayleigh_scatter + mie_scatter;
            vec3 integral_fms = (scattering_no_phase - scattering_no_phase * altitude_trans) / extinction;
            Fms += integral_fms * transmittance;

            transmittance *= altitude_trans;
        }

        if (planet_hit) {
            const vec3 terrain_albedo = vec3(0.217, 0.347, 0.594);
            vec3 hit_pos = ray_pos + max_dist * sample_dir;
            float h = length(hit_pos);
            vec3 up = hit_pos / h;
            float altitude = h - atmos.planet_radius;
            float theta = dot(sun_dir, up);
            vec3 sun_trans = sample_lut(atmos, c.transmittance_image_id, c.world.linear_sampler, altitude, theta);
            float light_theta = saturate(dot(up, sun_dir));
            L2 += sun_trans * transmittance * light_theta * (terrain_albedo / PI);
        }

        L2_sum += L2 / sample_count;
        Fms_sum += Fms / sample_count;
    }

    return L2_sum / (1.0 - Fms_sum);
}

[shader("compute")]
[numthreads(16, 16, 1)]
void cs_main(uint3 thread_id : SV_DispatchThreadID) {
    Atmosphere *atmos = &c.world->atmosphere;
    const let ms_image = ImageView(c.ms_image_id);

    float2 uv = float2(float2(thread_id.xy) + 0.5) / float2(c.ms_image_size);

    float altitude = lerp(atmos->planet_radius, atmos->atmos_radius, uv.y);
    float sun_cos_theta = uv.x * 2.0 - 1.0;
    vec3 sun_dir = vec3(0.0, sun_cos_theta, sqrt(1.0 - sun_cos_theta * sun_cos_theta));
    vec3 ray_pos = vec3(0.0, altitude, 0.0);

    ms_image.rw()[thread_id.xy] = float4(integrate(atmos, ray_pos, sun_dir), 1.0);
}
