import atmos;
import "../lorr";

[[vk::binding(0, 0)]] SamplerState sampler;
[[vk::binding(1, 0)]] RWTexture3D<float4> aerial_perspective_lut;
[[vk::binding(2, 0)]] Texture2D<float4> transmittance_lut;
[[vk::binding(3, 0)]] Texture2D<float4> multiscattering_lut;
[[vk::binding(4, 0)]] ConstantBuffer<Scene, ScalarDataLayout> scene;

[shader("compute")]
[numthreads(16, 16, 1)]
void cs_main(uint3 thread_id : SV_DispatchThreadID) {
    const let camera = scene.camera;
    const let atmos = scene.atmosphere;
    const let sun = scene.sun;

    float2 uv = (float2(thread_id.xy) + 0.5) / float2(atmos.aerial_perspective_lut_size.xy);
    float3 clip_space = float3(2.0 * uv - 1.0, 1.0);
    float4 world_space = mul(camera.inv_projection_view_mat, float4(clip_space, 1.0));
    float3 world_pos = world_space.xyz / world_space.w;
    float3 world_dir = normalize(world_pos - camera.position);
    float3 eye_pos = float3(0.0, camera.position.y, 0.0) *
                     CAMERA_SCALE_UNIT +
                     float3(0.0, atmos.planet_radius, 0.0);

    int step_count = int(max(1.0, float(thread_id.z + 1.0) * 2.0));
    float slice = ((float(thread_id.z) + 0.5) * (1.0 / atmos.aerial_perspective_lut_size.z));
    slice *= slice;
    slice *= atmos.aerial_perspective_lut_size.z;
    float t_max = slice * atmos.aerial_gain_per_slice;

    float3 ray_pos = eye_pos + t_max * world_dir;
    float view_height = length(ray_pos);

    if (view_height <= (atmos.planet_radius + PLANET_RADIUS_OFFSET)) {
        ray_pos = normalize(ray_pos) * (atmos.planet_radius + PLANET_RADIUS_OFFSET);
        world_dir = normalize(ray_pos - eye_pos);
        t_max = length(ray_pos - eye_pos);
    }

    float t_max_max = t_max;
    view_height = length(eye_pos);
    if (view_height >= atmos.atmos_radius) {
        float3 prev_ray_pos = eye_pos;
        if (!move_to_top_atmosphere(eye_pos, world_dir, atmos.atmos_radius)) {
            aerial_perspective_lut[thread_id] = 0.0;
            return;
        }
        float length_to_atmosphere = length(prev_ray_pos - eye_pos);
        if (t_max_max < length_to_atmosphere) {
            aerial_perspective_lut[thread_id] = 0.0;
            return;
        }

        t_max_max = max(0.0, t_max_max - length_to_atmosphere);
    }

    AtmosphereIntegrateInfo info = {};
    info.eye_pos = eye_pos;
    info.eye_dir = world_dir;
    info.sun_dir = sun.direction;
    info.sun_intensity = sun.intensity;
    info.max_integration_length = t_max_max;

    info.sampling.variable_sample_count = false;
    info.sampling.initial_sample_count = max(1.0, (float(thread_id.z) + 1.0) * 2.0);

    info.transmittance_image = transmittance_lut;
    info.multiscattering_image = multiscattering_lut;

    const let result = integrate_single_scattered_luminance(sampler, atmos, info);
    const float transmittance = dot(result.transmittance, float3(1.0f / 3.0f, 1.0f / 3.0f, 1.0f / 3.0f));
    aerial_perspective_lut[thread_id] = float4(result.luminance, transmittance);
}
