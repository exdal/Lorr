import atmos;
import "../lorr";

[[vk::binding(0, 0)]] SamplerState sampler;
[[vk::binding(1, 0)]] RWTexture2D<float4> sky_view_lut;
[[vk::binding(2, 0)]] Texture2D<float4> transmittance_lut;
[[vk::binding(3, 0)]] Texture2D<float4> multiscattering_lut;
[[vk::binding(4, 0)]] ConstantBuffer<Atmosphere, ScalarDataLayout> atmos;
[[vk::binding(5, 0)]] ConstantBuffer<Sun, ScalarDataLayout> sun;
[[vk::binding(6, 0)]] ConstantBuffer<Camera, ScalarDataLayout> camera;

[shader("compute")]
[numthreads(16, 16, 1)]
void cs_main(uint3 thread_id : SV_DispatchThreadID) {
    float2 tex_coord = (float2(thread_id.xy) + 0.5) / float2(atmos.sky_view_lut_size.xy);
    f32 u = fromSubUvsToUnit(tex_coord.x, f32(atmos.sky_view_lut_size.x));
    f32 v = fromSubUvsToUnit(tex_coord.y, f32(atmos.sky_view_lut_size.y));
    float3 eye_pos = float3(0.0, camera.position.y, 0.0) *
                 CAMERA_SCALE_UNIT +
                 float3(0.0, atmos->planet_radius, 0.0);
    f32 h = length(eye_pos);
    float horizon = lr::safe_sqrt(h * h - atmos.planet_radius * atmos.planet_radius);
    float cos_beta = horizon / h;
    float beta = acos(cos_beta);
    float zenith_horizon_angle = PI - beta;

    float view_zenith_cos_angle = 0.0;
    float light_view_cos_angle = -((u * u) * 2.0 - 1.0);
    // Non-linear parameterization
    if (v < 0.5) {
        f32 coord = 2.0 * v;
        coord = 1.0 - coord;
        coord *= coord;
        coord = 1.0 - coord;
        view_zenith_cos_angle = cos(zenith_horizon_angle * coord);
    } else {
        f32 coord = v * 2.0 - 1.0;
        coord *= coord;
        view_zenith_cos_angle = cos(zenith_horizon_angle + beta * coord);
    }

    float3 up_vec = eye_pos / h;
    float sun_zenith_cos_angle = dot(normalize(sun.direction), up_vec);
    float3 sun_dir = normalize(float3(lr::safe_sqrt(1.0 - sun_zenith_cos_angle * sun_zenith_cos_angle), sun_zenith_cos_angle, 0.0));

    float view_zenith_sin_angle = lr::safe_sqrt(1.0 - view_zenith_cos_angle * view_zenith_cos_angle);
    float3 eye_dir = float3(
        view_zenith_sin_angle * light_view_cos_angle,
        view_zenith_cos_angle,
        view_zenith_sin_angle * lr::safe_sqrt(1.0 - light_view_cos_angle * light_view_cos_angle)
    );

    if (!move_to_top_atmosphere(eye_pos, eye_dir, atmos.atmos_radius)) {
        sky_view_lut[thread_id.xy] = float4(0.0f);
        return;
    }

    AtmosphereIntegrateInfo info = {};
    info.eye_pos = eye_pos;
    info.eye_dir = eye_dir;
    info.sun_dir = sun_dir;
    info.sun_intensity = sun.intensity;

    const int sample_count = 32;
    info.sampling.variable_sample_count = true;
    info.sampling.min_sample_count = sample_count;
    info.sampling.max_sample_count = sample_count;

    info.transmittance_image = transmittance_lut;
    info.multiscattering_image = multiscattering_lut;

    const let result = integrate_single_scattered_luminance(sampler, atmos, info);
    const float transmittance = dot(result.transmittance, float3(1.0f / 3.0f, 1.0f / 3.0f, 1.0f / 3.0f));
    sky_view_lut[thread_id.xy] = float4(result.luminance, transmittance);
}

