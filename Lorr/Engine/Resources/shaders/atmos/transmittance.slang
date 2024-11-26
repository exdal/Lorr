import atmos;
import "../lorr";

[[vk::push_constant]] struct {
    World *world;
    float2 image_size;
    ImageView<float4> transmittance_image;
    u32 pad;
} c;

[shader("compute")]
[numthreads(16, 16, 1)]
void cs_main(uint3 thread_id : SV_DispatchThreadID) {
    Atmosphere *atmos = &c.world->atmosphere;
    float2 uv = float2(float2(thread_id.xy) + 0.5) / float2(c.image_size);

    float h = lr::safe_sqrt(
        atmos->atmos_radius * atmos->atmos_radius -
        atmos->planet_radius * atmos->planet_radius);
    float rho = h * uv.y;
    float lut_x = sqrt(rho * rho + atmos->planet_radius * atmos->planet_radius);
    float d_min = atmos->atmos_radius - lut_x;
    float d_max = rho + h;
    float d = d_min + uv.x * (d_max - d_min);
    float lut_y = d == 0.0 ? 1.0 : (h * h - rho * rho - d * d) / (2.0 * lut_x * d);
    lut_y = clamp(lut_y, -1.0, 1.0);

    float3 sun_dir = float3(0.0, sqrt(1.0 - lut_y * lut_y), lut_y);
    float3 ray_pos = float3(0.0, 0.0, lut_x);

    const float STEP_COUNT = 1000.0;
    float distance = lr::ray_sphere_intersect_nearest(ray_pos, sun_dir, atmos.atmos_radius).value;
    float distance_per_step = distance / STEP_COUNT;
    float3 optical_depth = float3(0.0);
    for (float i = 0.0; i < STEP_COUNT; i += 1.0) {
        ray_pos += sun_dir * distance_per_step;
        let ray_altitude = length(ray_pos) - atmos.planet_radius;
        const let medium = MediumScattering(atmos, ray_altitude);
        optical_depth += medium.extinction_sum * distance_per_step;
    }

    c.transmittance_image.store(thread_id.xy, float4(exp(-optical_depth), 1.0));
}
