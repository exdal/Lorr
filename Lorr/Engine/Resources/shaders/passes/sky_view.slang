module sky_view;

import std;
import gpu;
import sky;
import scene;

struct ShaderParameters {
    Sampler sampler;
    Image2D<f32x4> sky_transmittance_lut;
    Image2D<f32x4> sky_multiscattering_lut;
    ConstantBuffer<Atmosphere> atmosphere;
    ConstantBuffer<Camera> camera;
    StorageImage2D<f32x4, ImageFormat::RGBA16F> sky_view_lut;
};

[[shader("compute")]]
[[numthreads(16, 16, 1)]]
func cs_main(
    u32x2 thread_id : SV_DispatchThreadID,
    uniform ParameterBlock<ShaderParameters> params
) -> void {
    if (any(thread_id >= u32x2(params.atmosphere.sky_view_lut_size.xy))) {
        return;
    }

    let uv = f32x2(thread_id.xy) / f32x2(params.atmosphere.sky_view_lut_size.xy);
    var eye_pos = f32x3(0.0, params.atmosphere.eye_height, 0.0);
    let sky_params = uv_to_sky_view_lut_params(
        params.atmosphere.atmos_radius,
        params.atmosphere.planet_radius,
        params.atmosphere.sky_view_lut_size.xy,
        uv,
        params.atmosphere.eye_height);

    let view_zenith_angle = sky_params.x;
    let light_view_angle = sky_params.y;
    let eye_dir = f32x3(
        cos(light_view_angle) * sin(view_zenith_angle),
        cos(view_zenith_angle),  // Y is up
        sin(light_view_angle) * sin(view_zenith_angle),
    );

    if (!move_to_top_atmosphere(eye_pos, eye_dir, params.atmosphere.atmos_radius)) {
        params.sky_view_lut.Store(thread_id.xy, 0.0);
        return;
    }

    let up_vec = f32x3(0.0, 1.0, 0.0);
    let sun_zenith_cos_angle = dot(params.atmosphere.sun_direction, up_vec);
    let sun_dir = normalize(f32x3(std::safe_sqrt(1.0 - sun_zenith_cos_angle * sun_zenith_cos_angle), sun_zenith_cos_angle, 0.0));

    AtmosphereIntegrateInfo info = {};
    info.eye_pos = eye_pos;
    info.eye_dir = eye_dir;
    info.sun_dir = sun_dir;
    info.sun_intensity = params.atmosphere.sun_intensity;
    info.step_count = 32.0;
    info.eval_multiscattering = true;
    info.eval_planet_luminance = false;
    let result = integrate_single_scattered_luminance(
        info, params.atmosphere, params.sampler, params.sky_transmittance_lut, params.sky_multiscattering_lut);
    let inv_luminance = 1.0 / max(result.luminance, float3(1.0 / 1048576.0));
    let inv_mult = min(1048576.0, max(inv_luminance.x, max(inv_luminance.y, inv_luminance.z)));

    params.sky_view_lut.Store(thread_id.xy, f32x4(result.luminance * inv_mult, 1.0 / inv_mult));
}
