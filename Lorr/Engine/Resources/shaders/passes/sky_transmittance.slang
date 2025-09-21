module sky_transmittance;

import std;
import gpu;
import sky;
import scene;

struct ShaderParameters {
    StorageImage2D<f32x4, ImageFormat::RGBA16F> sky_transmittance_lut;
    ConstantBuffer<Environment> environment;
};

[[shader("compute")]]
[[numthreads(16, 16, 1)]]
func cs_main(
    u32x2 thread_id : SV_DispatchThreadID,
    uniform ParameterBlock<ShaderParameters> params
) -> void {
    f32x2 uv = f32x2(f32x2(thread_id.xy) + 0.5) / f32x2(params.environment.transmittance_lut_size.xy);
    f32 h = std::safe_sqrt(
        params.environment.atmos_atmos_radius * params.environment.atmos_atmos_radius
        - params.environment.atmos_planet_radius * params.environment.atmos_planet_radius);
    f32 rho = h * uv.y;
    f32 lut_x = sqrt(rho * rho + params.environment.atmos_planet_radius * params.environment.atmos_planet_radius);
    f32 d_min = params.environment.atmos_atmos_radius - lut_x;
    f32 d_max = rho + h;
    f32 d = d_min + uv.x * (d_max - d_min);
    f32 lut_y = d == 0.0 ? 1.0 : (h * h - rho * rho - d * d) / (2.0 * lut_x * d);
    lut_y = clamp(lut_y, -1.0, 1.0);

    f32x3 sun_dir = f32x3(0.0, sqrt(1.0 - lut_y * lut_y), lut_y);
    f32x3 ray_pos = f32x3(0.0, 0.0, lut_x);

    const f32 STEP_COUNT = 420.0;
    f32 distance = std::ray_sphere_intersect_nearest(ray_pos, sun_dir, params.environment.atmos_atmos_radius);
    f32 distance_per_step = distance / STEP_COUNT;
    f32x3 optical_depth = 0.0;
    for (f32 i = 0.0; i < STEP_COUNT; i += 1.0) {
        ray_pos += sun_dir * distance_per_step;
        let ray_altitude = length(ray_pos) - params.environment.atmos_planet_radius;
        let medium = MediumScattering(params.environment, ray_altitude);
        optical_depth += medium.extinction_sum * distance_per_step;
    }

    params.sky_transmittance_lut.Store(thread_id, f32x4(exp(-optical_depth), 1.0));
}
