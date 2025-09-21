// Credits:
// - https://github.com/Sunset-Flock/Timberdoodle/blob/a44dd4dd6b7f75e37be29a1178088c6b1b41b3dd/src/rendering/tasks/sky.glsl

import std;
import gpu;
import scene;
import sky;

typealias RWTextureCube<T:ITexelElement=float4, let sampleCount:int=0, let format:int=0> = _Texture<T, __ShapeCube, 0, 0, sampleCount, 1, 0, 0, format>;

struct ShaderParameters {
    Sampler sampler;
    Image2D<f32x4> sky_view_lut;
    ConstantBuffer<Environment> environment;
    ConstantBuffer<Camera> camera;
    RWTextureCube<f32x4> ibl_cube;
};

float radical_inverse_vdc(uint bits) {
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

f32x2 hammersley(uint i, uint n) {
    return f32x2(float(i + 1) / n, radical_inverse_vdc(i + 1));
}

mat3 CUBE_MAP_FACE_ROTATION(uint face) {
    switch (face) {
        case 0: return mat3(+0, +0, -1, +0, -1, +0, -1, +0, +0);
        case 1: return mat3(+0, +0, +1, +0, -1, +0, +1, +0, +0);
        case 2: return mat3(+1, +0, +0, +0, +0, -1, +0, +1, +0);
        case 3: return mat3(+1, +0, +0, +0, +0, +1, +0, -1, +0);
        case 4: return mat3(+1, +0, +0, +0, -1, +0, +0, +0, -1);
        default: return mat3(-1, +0, +0, +0, -1, +0, +0, +0, +1);
    }
}

static uint _rand_state;
void rand_seed(uint seed) {
    _rand_state = seed;
}

float rand() {
    // https://www.pcg-random.org/
    _rand_state = _rand_state * 747796405u + 2891336453u;
    uint result = ((_rand_state >> ((_rand_state >> 28u) + 4u)) ^ _rand_state) * 277803737u;
    result = (result >> 22u) ^ result;
    return result / 4294967295.0;
}

float rand_normal_dist() {
    float theta = 2.0 * PI * rand();
    float rho = sqrt(-2.0 * log(rand()));
    return rho * cos(theta);
}

f32x3 rand_dir() {
    return normalize(f32x3(
        rand_normal_dist(),
        rand_normal_dist(),
        rand_normal_dist()));
}

f32x3 rand_hemi_dir(f32x3 nrm) {
    f32x3 result = rand_dir();
    return result * sign(dot(nrm, result));
}

#define IBL_CUBE_RES 32
#define IBL_CUBE_X 2
#define IBL_CUBE_Y 2

[[shader("compute")]]
[[numthreads(IBL_CUBE_X, IBL_CUBE_Y, IBL_CUBE_RES)]]
func cs_main(
    u32x3 thread_id : SV_DispatchThreadID,
    u32x3 group_id : SV_GroupID,
    uniform ParameterBlock<ShaderParameters> params,
    uniform u32 frame_index
) -> void {
    let wg_base_pix_pos = group_id.xy * u32x2(IBL_CUBE_X, IBL_CUBE_Y);
    let sg_index = std::subgroup_id();
    let sg_pix_pos = wg_base_pix_pos + u32x2(sg_index % IBL_CUBE_X, sg_index / IBL_CUBE_X);
    let sg_thread_id = WaveGetLaneIndex();
    let face = group_id.z;
    let uv = (f32x2(sg_pix_pos) + 0.5) / IBL_CUBE_RES;
    let output_dir = normalize(mul(CUBE_MAP_FACE_ROTATION(face), f32x3(uv * 2.0 - 1.0, -1.0)));

    let up = f32x3(0.0, 1.0, 0.0);
    var eye_altitude = params.camera.position.y * CAMERA_SCALE_UNIT;
    eye_altitude += params.environment.atmos_planet_radius + PLANET_RADIUS_OFFSET;
    let eye_pos = f32x3(0.0, eye_altitude, 0.0);

    // these values are hardcoded
    let sample_count = 128;
    let subgroup_size = 32;
    let iter_count = sample_count / subgroup_size;
    let global_thread_index = (thread_id.x * IBL_CUBE_RES * IBL_CUBE_RES + thread_id.y * IBL_CUBE_RES + thread_id.z);
    let seed = global_thread_index + frame_index * IBL_CUBE_RES * IBL_CUBE_RES * 6;
    var accumulated_result = f32x3(0.0);
    for (uint i = 0; i < iter_count; ++i) {
        rand_seed((i * subgroup_size + sg_thread_id + seed * sample_count));
        let input_dir = rand_hemi_dir(output_dir);
        let result = get_atmosphere_illuminance_along_ray(
            eye_pos, input_dir, params.environment.sun_direction, params.environment, params.sampler, params.sky_view_lut);

        let cos_weighed_result = result * dot(output_dir, input_dir);
        accumulated_result += std::subgroup_inclusive_add(cos_weighed_result);
    }

    if (sg_thread_id == 31) {
        let this_frame_luminance = accumulated_result / sample_count;
        let compressed_accumulated_luminance = params.ibl_cube.Load(i32x3(sg_pix_pos, group_id.z));
        // Could be nan for some reason
        let unsafe_accumulated_luminance = compressed_accumulated_luminance.rgb * compressed_accumulated_luminance.a;
        let accumulated_luminance = isnan(unsafe_accumulated_luminance.x) ? 0.0 : unsafe_accumulated_luminance;

        let luminance = 0.995 * accumulated_luminance + 0.005 * this_frame_luminance;
        let inv_luminance = 1.0 / max(luminance, 1.0 / 1048576.0);
        let inv_mult = min(1048576.0, max(inv_luminance.x, max(inv_luminance.y, inv_luminance.z)));
        params.ibl_cube.Store(i32x3(sg_pix_pos, group_id.z), f32x4(luminance * inv_mult, 1.0 / inv_mult));
    }
}