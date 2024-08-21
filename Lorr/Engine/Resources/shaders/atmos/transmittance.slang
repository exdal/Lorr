import "../lorr";
import "common";

[[vk::push_constant]] struct {
    lr::World *world;
    float2 image_size;
    ImageViewID target_image_id;
} c;

[shader("compute")]
[numthreads(16, 16, 1)]
void cs_main(uint3 thread_id : SV_DispatchThreadID) {
    let target_image = RWTexture2D<float4>::get(c.target_image_id);
    lr::Atmosphere *atmos = &c.world->atmosphere;
    float2 uv = float2(thread_id.xy) / c.image_size;
    float altitude = lerp(atmos.planet_radius, atmos.atmos_radius, uv.y);
    float sun_cos_theta = uv.x * 2.0 - 1.0;
    float3 sun_dir = float3(0.0, sun_cos_theta, sqrt(1.0 - sun_cos_theta * sun_cos_theta));
    float3 ray_pos = float3(0.0, altitude, 0.0);

    const float STEP_COUNT = 40.0;
    float distance = 0.0;
    ray_hit_sphere(ray_pos, sun_dir, atmos.atmos_radius, distance);
    float distance_per_step = distance / STEP_COUNT;
    float3 optical_depth = float3(0.0);
    for (float i = 0.0; i < STEP_COUNT; i += 1.0) {
        ray_pos += sun_dir * distance_per_step;
        let ray_altitude = length(ray_pos) - atmos.planet_radius;
        optical_depth += extinction_sum(atmos, ray_altitude) * distance_per_step;
    }

    target_image[thread_id.xy] = float4(exp(-optical_depth), 1.0);
}
