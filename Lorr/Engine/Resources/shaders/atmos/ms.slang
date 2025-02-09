import atmos;
import "../lorr";
import "../noise/hemisphere";

[[vk::binding(0, 0)]] SamplerState sampler;
[[vk::binding(1, 0)]] Texture2D<float4> transmittance_lut;
[[vk::binding(2, 0)]] RWTexture2D<float4> multiscattering_lut;
[[vk::binding(3, 0)]] ConstantBuffer<Atmosphere, ScalarDataLayout> atmos;
[[vk::binding(4, 0)]] ConstantBuffer<Sun, ScalarDataLayout> sun;

[shader("compute")]
[numthreads(16, 16, 1)]
void cs_main(uint3 thread_id : SV_DispatchThreadID) {
    float2 uv = float2(float2(thread_id.xy) + 0.5) / float2(atmos.multiscattering_lut_size.xy);

    float altitude = lerp(atmos.planet_radius, atmos.atmos_radius, uv.y);
    float sun_cos_theta = uv.x * 2.0 - 1.0;
    float3 sun_dir = float3(0.0, sun_cos_theta, sqrt(1.0 - sun_cos_theta * sun_cos_theta));
    float3 ray_pos = float3(0.0, altitude, 0.0);

    AtmosphereIntegrateInfo info = {};
    info.eye_pos = ray_pos;
    info.sun_dir = sun_dir;
    //info.sun_intensity = sun->intensity;
    info.eval_mie_rayleigh_phase = false;
    info.eval_planet_luminance = true;
    info.sampling.variable_sample_count = false;
    info.sampling.initial_sample_count = 32;

    info.transmittance_image = transmittance_lut;

    float3 luminance = float3(0.0);
    float3 multi_scattering_as_1 = float3(0.0);
    for (int i = 0; i < 64; i++) {
        info.eye_dir = Noise::hemisphere64[i];
        const let result = integrate_single_scattered_luminance(sampler, atmos, info);
        multi_scattering_as_1 += result.multiscattering_as_1;
        luminance += result.luminance;
    }

    const float sphere_solid_angle = 4.0f * PI;
    const float isotropic_phase = 1.0f / sphere_solid_angle;
    const float inv_sample_count = 1.0 / 64.0;
    luminance *= sphere_solid_angle * inv_sample_count;
    multi_scattering_as_1 *= inv_sample_count;
    float3 scattered_luminance = luminance * isotropic_phase;
    float3 f_ms = 1.0 / (1.0 - multi_scattering_as_1);

    multiscattering_lut[thread_id.xy] = float4(scattered_luminance * f_ms, 1.0);
}
