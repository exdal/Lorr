module sky_multiscattering;

import std;
import gpu;
import sky;
import scene;

import embedded.hemisphere;

struct PushConstants {
    Sampler sampler;
    Image2D<f32x4> sky_transmittance_lut;
    StorageImage2D<f32x4> sky_multiscattering_lut;
    Atmosphere *atmosphere;
};
[[vk::push_constant]] PushConstants C;

[[shader("compute")]]
[[numthreads(16, 16, 1)]]
func cs_main(u32x3 thread_id : SV_DispatchThreadID) {
    f32x2 uv = f32x2(f32x2(thread_id.xy) + 0.5) / f32x2(C.atmosphere->multiscattering_lut_size.xy);

    f32 altitude = lerp(C.atmosphere->planet_radius, C.atmosphere->atmos_radius, uv.y);
    f32 sun_cos_theta = uv.x * 2.0 - 1.0;
    f32x3 sun_dir = f32x3(0.0, sun_cos_theta, std::safe_sqrt(1.0 - sun_cos_theta * sun_cos_theta));
    f32x3 ray_pos = f32x3(0.0, altitude, 0.0);

    AtmosphereIntegrateInfo info;
    info.eye_pos = ray_pos;
    info.sun_dir = sun_dir;
    info.eval_mie_rayleigh_phase = false;
    info.eval_planet_luminance = true;
    info.sampling.variable_sample_count = false;
    info.sampling.initial_sample_count = 32;

    info.transmittance_image = C.sky_transmittance_lut;

    f32x3 luminance = 0.0;
    f32x3 multi_scattering_as_1 = 0.0;
    for (int i = 0; i < 64; i++) {
        info.eye_dir = HEMISPHERE_64[i];
        const let result = integrate_single_scattered_luminance(C.atmosphere, C.sampler, info);
        multi_scattering_as_1 += result.multiscattering_as_1;
        luminance += result.luminance;
    }

    const let sphere_solid_angle = 4.0f * PI;
    const let isotropic_phase = 1.0f / sphere_solid_angle;
    const let inv_sample_count = 1.0 / 64.0;
    luminance *= sphere_solid_angle * inv_sample_count;
    multi_scattering_as_1 *= inv_sample_count;
    f32x3 scattered_luminance = luminance * isotropic_phase;
    f32x3 f_ms = 1.0 / (1.0 - multi_scattering_as_1);

    C.sky_multiscattering_lut.store(thread_id.xy, f32x4(scattered_luminance * f_ms, 1.0));
}
