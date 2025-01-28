import atmos;
import "../lorr";

[[vk::binding(0, 0)]] SamplerState sampler;
[[vk::binding(1, 0)]] RWTexture3D<float4> aerial_perspective_lut;
[[vk::binding(2, 0)]] Texture2D<float4> transmittance_lut;
[[vk::binding(3, 0)]] Texture2D<float4> multiscattering_lut;
[[vk::binding(4, 0)]] ConstantBuffer<Atmosphere, ScalarDataLayout> atmos;
[[vk::binding(5, 0)]] ConstantBuffer<Sun, ScalarDataLayout> sun;
[[vk::binding(6, 0)]] ConstantBuffer<Camera, ScalarDataLayout> camera;

[shader("compute")]
[numthreads(16, 16, 1)]
void cs_main(uint3 thread_id : SV_DispatchThreadID) {
    float2 uv = (float2(thread_id.xy) + 0.5) / float2(atmos.aerial_perspective_lut_size.xy);
    float3 clip_space = float3(2.0 * uv - 1.0, 0.5);
    float4 world_space = mul(camera.inv_projection_view_mat, float4(clip_space, 1.0));
    float3 world_pos = world_space.xyz / world_space.w;
    float3 eye_dir = normalize(world_pos - camera.position);
    float3 eye_pos = float3(0.0, camera.position.y, 0.0) *
                     CAMERA_SCALE_UNIT +
                     float3(0.0, atmos.planet_radius, 0.0);

    int step_count = int(max(1.0, float(thread_id.z + 1.0) * 2.0));
    float slice = ((float(thread_id.z) + 0.5) * (1.0 / atmos.aerial_perspective_lut_size.z));
    slice *= slice;
    slice *= atmos.aerial_perspective_lut_size.z;
    float t_max = slice * atmos.aerial_km_per_slice;

    float3 ray_pos = eye_pos;
    float3 voxel_world_pos = ray_pos + t_max * eye_dir;
    float voxel_height = length(voxel_world_pos);
    float3 eye_to_voxel = voxel_world_pos - eye_pos;
    float eye_to_voxel_height = length(eye_to_voxel);
    float3 eye_to_voxel_dir = eye_to_voxel / eye_to_voxel_height;

    const let planet_intersection = lr::ray_sphere_intersect_nearest(eye_pos, eye_to_voxel_dir, atmos.planet_radius);
    const bool under_ground = voxel_height < atmos.planet_radius;
    const bool below_horizon = planet_intersection.hasValue && eye_to_voxel_height > planet_intersection.value;

    if (under_ground || below_horizon) {
        eye_pos += normalize(eye_pos) * 0.02;
        float3 voxel_world_dir = normalize(voxel_world_pos);
        float3 eye_proj_on_ground = normalize(eye_pos) * atmos.planet_radius;
        float3 voxel_proj_on_ground = voxel_world_dir * atmos.planet_radius;
        float3 voxel_ground_to_ray_start = eye_pos - voxel_proj_on_ground;
        if (below_horizon && dot(normalize(voxel_ground_to_ray_start), voxel_world_dir) < PLANET_RADIUS_OFFSET) {
            float3 middle_point = 0.5 * (eye_proj_on_ground + voxel_proj_on_ground);
            float middle_point_height = length(middle_point);
            float3 middle_point_on_ground = normalize(middle_point) * atmos.planet_radius;
            voxel_world_pos = eye_pos + 2.0 * (middle_point_on_ground - eye_pos);
        } else if (under_ground) {
            voxel_world_pos = voxel_world_dir * atmos.planet_radius;
        }

        eye_dir = normalize(voxel_world_pos - eye_pos);
        ray_pos = eye_pos;
        t_max = length(voxel_world_pos - ray_pos);
    }

    float t_max_max = t_max;
    float ray_height = length(ray_pos);
    if (ray_height >= atmos.atmos_radius) {
        float3 prev_ray_pos = ray_pos;
        if (!move_to_top_atmosphere(ray_pos, eye_dir, atmos.atmos_radius)) {
            aerial_perspective_lut[thread_id] = 0.0;
            return;
        }
        float length_to_atmosphere = length(prev_ray_pos - ray_pos);
        if (t_max_max < length_to_atmosphere) {
            aerial_perspective_lut[thread_id] = 0.0;
            return;
        }
        t_max_max = max(0.0, t_max_max - length_to_atmosphere);
    }

    AtmosphereIntegrateInfo info = {};
    info.eye_pos = eye_pos;
    info.eye_dir = eye_dir;
    info.sun_dir = sun.direction;
    info.sun_intensity = sun.intensity;
    info.lut_sampler = sampler;
    info.transmittance_image = transmittance_lut;
    info.multiscattering_image = multiscattering_lut;
    info.max_integration_length = t_max_max;

    info.sampling.variable_sample_count = false;
    info.sampling.initial_sample_count = max(1.0, (float(thread_id.z) + 1.0) * 2.0);

    const let result = integrate_single_scattered_luminance(atmos, info);
    const float transmittance = dot(result.transmittance, float3(1.0f / 3.0f, 1.0f / 3.0f, 1.0f / 3.0f));
    aerial_perspective_lut[thread_id] = float4(result.luminance, transmittance);
}
