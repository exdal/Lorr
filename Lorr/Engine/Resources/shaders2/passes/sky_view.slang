module sky_view;

import std;
import gpu;
import sky;
import scene;

struct PushConstants {
    Sampler sampler;
    Image2D<f32x4> sky_transmittance_lut;
    Image2D<f32x4> sky_multiscattering_lut;
    StorageImage2D<f32x4> sky_view_lut;
    Atmosphere *atmosphere;
    Sun *sun;
    Camera *camera;
};
[[vk::push_constant]] PushConstants C;

[[shader("compute")]]
[[numthreads(16, 16, 1)]]
func cs_main(u32x3 thread_id : SV_DispatchThreadID) {
    f32x2 uv = f32x2(f32x2(thread_id.xy) + 0.5) / f32x2(C.atmosphere->sky_view_lut_size.xy);
    f32x3 eye_pos = f32x3(0.0, C.camera->position.y, 0.0) +
                    CAMERA_SCALE_UNIT +
                    f32x3(0.0, C.atmosphere->planet_radius, 0.0);
    f32 h = length(eye_pos);
    f32 horizon = std::safe_sqrt(h * h - C.atmosphere->planet_radius * C.atmosphere->planet_radius);
    f32 cos_beta = horizon / h;
    f32 beta = acos(cos_beta);
    f32 zenith_horizon_angle = PI - beta;

    f32 view_zenith_cos_angle = 0.0;
    f32 light_view_cos_angle = -((uv.x * uv.x) * 2.0 - 1.0);
    // Non-linear parameterization
    if (uv.y < 0.5) {
        f32 coord = 2.0 * uv.y;
        coord = 1.0 - coord;
        coord *= coord;
        coord = 1.0 - coord;
        view_zenith_cos_angle = cos(zenith_horizon_angle * coord);
    } else {
        f32 coord = uv.y * 2.0 - 1.0;
        coord *= coord;
        view_zenith_cos_angle = cos(zenith_horizon_angle + beta * coord);
    }

    f32x3 up_vec = eye_pos / h;
    f32 sun_zenith_cos_angle = dot(normalize(C.sun->direction), up_vec);
    f32x3 sun_dir = normalize(f32x3(std::safe_sqrt(1.0 - sun_zenith_cos_angle * sun_zenith_cos_angle), sun_zenith_cos_angle, 0.0));

    f32 view_zenith_sin_angle = std::safe_sqrt(1.0 - view_zenith_cos_angle * view_zenith_cos_angle);
    f32x3 eye_dir = f32x3(
        view_zenith_sin_angle * light_view_cos_angle,
        view_zenith_cos_angle,
        view_zenith_sin_angle * std::safe_sqrt(1.0 - light_view_cos_angle * light_view_cos_angle));

    if (!move_to_top_atmosphere(eye_pos, eye_dir, C.atmosphere->atmos_radius)) {
        C.sky_view_lut.store(thread_id.xy, 0.0);
        return;
    }

    AtmosphereIntegrateInfo info;
    info.eye_pos = eye_pos;
    info.eye_dir = eye_dir;
    info.sun_dir = sun_dir;
    info.sun_intensity = C.sun->intensity;

    const int sample_count = 32;
    info.sampling.variable_sample_count = true;
    info.sampling.min_sample_count = sample_count;
    info.sampling.max_sample_count = sample_count;

    info.transmittance_image = C.sky_transmittance_lut;
    info.multiscattering_image = C.sky_multiscattering_lut;

    const let result = integrate_single_scattered_luminance(C.atmosphere, C.sampler, info);
    const let transmittance = dot(result.transmittance, f32x3(1.0f / 3.0f, 1.0f / 3.0f, 1.0f / 3.0f));

    C.sky_view_lut.store(thread_id.xy, f32x4(result.luminance, transmittance));
}
