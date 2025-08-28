module sky_view;

import std;
import gpu;
import sky;
import scene;

struct ShaderParameters {
    Sampler sampler;
    Image2D<f32x4> sky_transmittance_lut;
    Image2D<f32x4> sky_multiscattering_lut;
    ConstantBuffer<Environment> environment;
    ConstantBuffer<Camera> camera;
    StorageImage2D<f32x4, ImageFormat::RGBA16F> sky_view_lut;
};

[[shader("compute")]]
[[numthreads(16, 16, 1)]]
func cs_main(
    u32x3 thread_id : SV_DispatchThreadID,
    uniform ParameterBlock<ShaderParameters> params
) -> void {
    let uv = f32x2(thread_id.xy) / f32x2(params.environment.sky_view_lut_size.xy);
    var eye_altitude = params.camera.position.y * CAMERA_SCALE_UNIT;
    eye_altitude += params.environment.atmos_planet_radius + PLANET_RADIUS_OFFSET;
    var eye_pos = f32x3(0.0, eye_altitude, 0.0);
    let eye_dir = uv_to_sky_view_lut_params(
        params.environment.atmos_planet_radius,
        params.environment.sky_view_lut_size.xy,
        uv,
        eye_altitude);

    if (!move_to_top_atmosphere(eye_pos, eye_dir, params.environment.atmos_atmos_radius)) {
        params.sky_view_lut.Store(thread_id.xy, 0.0);
        return;
    }

    let up_vec = f32x3(0.0, 1.0, 0.0);
    let sun_zenith_cos_angle = dot(normalize(params.environment.sun_direction), up_vec);
    let sun_dir = normalize(f32x3(std::safe_sqrt(1.0 - sun_zenith_cos_angle * sun_zenith_cos_angle), sun_zenith_cos_angle, 0.0));

    AtmosphereIntegrateInfo info;
    info.eye_pos = eye_pos;
    info.eye_dir = eye_dir;
    info.sun_dir = sun_dir;
    info.sun_intensity = params.environment.sun_intensity;

    let sample_count = 48;
    info.sampling.variable_sample_count = true;
    info.sampling.min_sample_count = sample_count;
    info.sampling.max_sample_count = sample_count;

    info.transmittance_image = params.sky_transmittance_lut;
    info.multiscattering_image = params.sky_multiscattering_lut;
    info.eval_multiscattering = true;

    let result = integrate_single_scattered_luminance(params.environment, params.sampler, info);
    let transmittance = dot(result.transmittance, 1.0 / 3.0);

    params.sky_view_lut.Store(thread_id.xy, f32x4(result.luminance, transmittance));
}
