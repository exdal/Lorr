module sky_multiscattering;

import std;
import gpu;
import sky;
import scene;

import embedded.hemisphere;

struct ShaderParameters {
    Sampler sampler;
    Image2D<f32x4> sky_transmittance_lut;
    ConstantBuffer<Atmosphere> atmosphere;
    StorageImage2D<f32x4, ImageFormat::RGBA16F> sky_multiscattering_lut;
};

#define GOLDEN_RATIO 1.6180339
#define SAMPLE_COUNT 64

static groupshared AtmosphereLuminance luminance_shared[2];

[[shader("compute")]]
[[numthreads(1, 1, SAMPLE_COUNT)]]
func cs_main(
    u32x3 thread_id : SV_DispatchThreadID,
    u32 group_index : SV_GroupIndex,
    uniform ParameterBlock<ShaderParameters> params
) -> void {
    let i = f32(thread_id.z);
    var uv = f32x2(f32x2(thread_id.xy) + 0.5) / f32x2(params.atmosphere.multiscattering_lut_size.xy);
    uv = from_sub_uvs_to_unit(uv, f32x2(params.atmosphere.multiscattering_lut_size.xy));

    let atmosphere_thickness = params.atmosphere.atmos_radius - params.atmosphere.planet_radius;
    let altitude = params.atmosphere.planet_radius + uv.y * atmosphere_thickness + PLANET_RADIUS_OFFSET;

    let sun_zenith_angle = uv.x * 2.0 - 1.0;
    let sun_dir = f32x3(0.0, sun_zenith_angle, std::safe_sqrt(saturate(1.0 - sun_zenith_angle * sun_zenith_angle)));
    let eye_pos = f32x3(0.0, altitude, 0.0);

    let phi = acos(1.0 - 2.0 * (i + 0.5) / f32(SAMPLE_COUNT));
    let theta = TAU * i / GOLDEN_RATIO;
    let eye_dir = f32x3(
        sin(phi) * cos(theta),
        cos(phi),
        sin(phi) * sin(theta),
    );

    AtmosphereIntegrateInfo info = {};
    info.eye_pos = eye_pos;
    info.eye_dir = eye_dir;
    info.sun_dir = sun_dir;
    info.eval_planet_luminance = true;
    info.eval_multiscattering = false;
    info.step_count = 32.0;
    var result = integrate_single_scattered_luminance(info, params.atmosphere, params.sampler, params.sky_transmittance_lut, params.sky_transmittance_lut);
    result.multiscattering_as_1 = WaveActiveSum(result.multiscattering_as_1);
    result.luminance = WaveActiveSum(result.luminance);

    GroupMemoryBarrierWithGroupSync();

    if (WaveIsFirstLane()) {
        let index = group_index > 0 ? 1 : 0;
        luminance_shared[index] = result;
    }

    GroupMemoryBarrierWithGroupSync();

    if (group_index == 0) {
        let ms_sum = (luminance_shared[0].multiscattering_as_1 + luminance_shared[1].multiscattering_as_1) / f32(SAMPLE_COUNT);
        let luminance_sum = (luminance_shared[0].luminance + luminance_shared[1].luminance) / f32(SAMPLE_COUNT);
        let luminance = luminance_sum * (1.0 / (1.0 - ms_sum));
        params.sky_multiscattering_lut.Store(thread_id.xy, f32x4(luminance, 1.0));
    }
}
