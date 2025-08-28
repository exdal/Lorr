module sky_aerial_perspective;

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
    StorageImage3D<f32x4, ImageFormat::RGBA16F> sky_aerial_perspective_lut;
};

[[shader("compute")]]
[[numthreads(16, 16, 1)]]
func cs_main(
    u32x3 thread_id : SV_DispatchThreadID,
    uniform ParameterBlock<ShaderParameters> params
) -> void {
    let lut_size = params.environment.aerial_perspective_lut_size;
    let uv = f32x2(f32x2(thread_id.xy) + 0.5) / f32x2(lut_size.xy);
    let NDC = f32x3(2.0 * uv - 1.0, 1.0);
    let world_pos_h = mul(params.camera.inv_projection_view_mat, f32x4(NDC, 1.0));
    let world_pos = world_pos_h.xyz / world_pos_h.w;
    let world_dir = normalize(world_pos - params.camera.position);
    var eye_altitude = params.camera.position.y * CAMERA_SCALE_UNIT;
    eye_altitude += params.environment.atmos_planet_radius + PLANET_RADIUS_OFFSET;
    var eye_pos = f32x3(0.0, eye_altitude, 0.0);

    f32 slice = ((f32(thread_id.z) + 0.5) * (1.0 / lut_size.z));
    slice *= slice;
    slice *= lut_size.z;

    let step_count = int(max(1.0, f32(thread_id.z + 1.0) * 2.0));
    let per_slice_depth = f32(lut_size.x / lut_size.z);
    let start_depth = params.environment.atmos_aerial_perspective_start_km * INV_CAMERA_SCALE_UNIT;
    var t_max = slice * per_slice_depth;

    let start_pos = eye_pos + start_depth * world_dir;
    let ray_pos = start_pos + t_max * world_dir;

    var view_height = 0.0;
    /*
    view_height = length(ray_pos);
    if (view_height <= (C.atmosphere.planet_radius + PLANET_RADIUS_OFFSET)) {
        ray_pos = normalize(ray_pos) * (C.atmosphere.planet_radius + PLANET_RADIUS_OFFSET);
        world_dir = normalize(ray_pos - eye_pos);
        t_max = length(ray_pos - eye_pos);
    }
    */

    f32 t_max_max = t_max;
    view_height = eye_altitude;
    if (view_height >= params.environment.atmos_atmos_radius) {
        f32x3 prev_ray_pos = eye_pos;
        if (!move_to_top_atmosphere(eye_pos, world_dir, params.environment.atmos_atmos_radius)) {
            params.sky_aerial_perspective_lut.Store(thread_id, 0.0);
            return;
        }
        f32 length_to_atmosphere = length(prev_ray_pos - eye_pos);
        if (t_max_max < length_to_atmosphere) {
            params.sky_aerial_perspective_lut.Store(thread_id, 0.0);
            return;
        }

        t_max_max = max(0.0, t_max_max - length_to_atmosphere);
    }

    AtmosphereIntegrateInfo info;
    info.eye_pos = eye_pos;
    info.eye_dir = world_dir;
    info.sun_dir = params.environment.sun_direction;
    info.sun_intensity = params.environment.sun_intensity;
    info.max_integration_length = t_max_max;
    info.eval_planet_luminance = false;
    info.eval_multiscattering = true;

    info.sampling.variable_sample_count = false;
    info.sampling.initial_sample_count = max(1.0, (f32(thread_id.z) + 1.0) * 2.0);

    info.transmittance_image = params.sky_transmittance_lut;
    info.multiscattering_image = params.sky_multiscattering_lut;

    let result = integrate_single_scattered_luminance(params.environment, params.sampler, info);
    let transmittance = dot(result.transmittance, f32x3(1.0f / 3.0f));
    params.sky_aerial_perspective_lut.Store(thread_id, f32x4(result.luminance, transmittance));
}
