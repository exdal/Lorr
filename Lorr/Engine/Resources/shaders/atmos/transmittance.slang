import atmos;
import "../lorr";

[[vk::binding(0, 0)]] RWTexture2D<float4> transmittance_image;
[[vk::binding(1, 0)]] ConstantBuffer<Scene, ScalarDataLayout> scene;

[shader("compute")]
[numthreads(16, 16, 1)]
void cs_main(uint3 thread_id : SV_DispatchThreadID) {
    const let atmosphere = scene.atmosphere;
    float2 uv = float2(float2(thread_id.xy) + 0.5) / float2(atmosphere.transmittance_lut_size.xy);
    float h = lr::safe_sqrt(
        atmosphere.atmos_radius * atmosphere.atmos_radius -
        atmosphere.planet_radius * atmosphere.planet_radius);
    float rho = h * uv.y;
    float lut_x = sqrt(rho * rho + atmosphere.planet_radius * atmosphere.planet_radius);
    float d_min = atmosphere.atmos_radius - lut_x;
    float d_max = rho + h;
    float d = d_min + uv.x * (d_max - d_min);
    float lut_y = d == 0.0 ? 1.0 : (h * h - rho * rho - d * d) / (2.0 * lut_x * d);
    lut_y = clamp(lut_y, -1.0, 1.0);

    float3 sun_dir = float3(0.0, sqrt(1.0 - lut_y * lut_y), lut_y);
    float3 ray_pos = float3(0.0, 0.0, lut_x);

    const float STEP_COUNT = 1000.0;
    float distance = lr::ray_sphere_intersect_nearest(ray_pos, sun_dir, atmosphere.atmos_radius).value;
    float distance_per_step = distance / STEP_COUNT;
    float3 optical_depth = float3(0.0);
    for (float i = 0.0; i < STEP_COUNT; i += 1.0) {
        ray_pos += sun_dir * distance_per_step;
        let ray_altitude = length(ray_pos) - atmosphere.planet_radius;
        const let medium = MediumScattering(atmosphere, ray_altitude);
        optical_depth += medium.extinction_sum * distance_per_step;
    }

    transmittance_image[thread_id.xy] = float4(exp(-optical_depth), 1.0);
}
