import atmos;
import "../lorr";

[[vk::binding(0, 0)]] SamplerState sampler;
[[vk::binding(1, 0)]] Texture2D<float4> transmittance_lut;
[[vk::binding(2, 0)]] Texture2D<float4> sky_view_lut;
[[vk::binding(3, 0)]] ConstantBuffer<Scene, ScalarDataLayout> scene;

struct VertexOutput {
    float4 position     : SV_Position;
    float2 tex_coord    : TEXCOORD;
};

[shader("vertex")]
VertexOutput vs_main(u32 vertex_id : SV_VertexID) {
    VertexOutput output;
    output.tex_coord = float2((vertex_id << 1) & 2, vertex_id & 2);
    output.position = float4(2.0 * output.tex_coord - 1.0, 1.0, 1.0);

    return output;
}

float3 draw_sun(float3 world_dir, float3 sun_dir, float radius) {
    const float min_cos_theta = cos(radius * PI / 180.0);

    float cosTheta = dot(world_dir, sun_dir);
    if (cosTheta >= min_cos_theta) {
        return float3(1.0);
    }

    float offset = min_cos_theta - cosTheta;
    float gaussianBloom = exp(-offset * 50000.0) * 0.5;
    float invBloom = 1.0/(0.02 + offset * 300.0) * 0.01;
    return float3(gaussianBloom + invBloom);
}

[shader("fragment")]
float4 fs_main(VertexOutput input) : SV_TARGET {
    const let camera = scene.camera;
    const let sun = scene.sun;
    const let atmos = scene.atmosphere;

    float3 clip_space = float3(input.tex_coord * 2.0 - 1.0, 1.0);
    float4 clip_pos = mul(camera.inv_projection_view_mat, float4(clip_space, 1.0));
    float3 clip_dir = clip_pos.xyz / clip_pos.w;
    float3 world_dir = normalize(clip_dir - camera.position);
    float3 world_pos = float3(0.0, camera.position.y, 0.0) *
                       CAMERA_SCALE_UNIT +
                       float3(0.0, atmos.planet_radius, 0.0);

    const let planet_intersection = lr::ray_sphere_intersect_nearest(world_pos, world_dir, atmos.planet_radius);

    float h = length(world_pos);
    float3 up_vec = normalize(world_pos);

    float3 sun_dir = normalize(sun.direction);
    float3 right_vec = normalize(cross(up_vec, world_dir));
    float3 forward_vec = normalize(cross(right_vec, up_vec));
    float2 light_on_plane = normalize(float2(dot(sun_dir, forward_vec), dot(sun_dir, right_vec)));

    float view_zenith_cos_angle = dot(world_dir, up_vec);
    float light_view_cos_angle = light_on_plane.x;

    float2 uv = get_sky_view_lut_uv(atmos, planet_intersection.hasValue, view_zenith_cos_angle, light_view_cos_angle, h);
    uv.x = fromUnitToSubUvs(uv.x, f32(atmos.sky_view_lut_size.x));
    uv.y = fromUnitToSubUvs(uv.y, f32(atmos.sky_view_lut_size.y));
    float3 luminance = sky_view_lut.Sample(sampler, uv).rgb;

    float sun_cos_theta = dot(sun.direction, up_vec);
    float2 transmittance_uv = get_transmittance_lut_uv(atmos, float2(h, sun_cos_theta));
    float3 sun_transmittance = transmittance_lut.Sample(sampler, transmittance_uv).rgb;

    if (!planet_intersection.hasValue) {
        luminance += draw_sun(world_dir, sun.direction, 1.0) * sun.intensity * sun_transmittance;
    }

    return float4(luminance, 1.0);
}


