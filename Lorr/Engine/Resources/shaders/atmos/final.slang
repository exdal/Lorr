import atmos;
import "../lorr";

[[vk::push_constant]] struct {
    World *world;
    SampledImage sky_view_lut;
    ImageView transmittance_lut;
    u32 pad;
} c;

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
    Camera *camera = &c.world.cameras[c.world.active_camera_index];
    Atmosphere *atmos = &c.world.atmosphere;
    Sun *sun = &c.world.sun;
    
    float3 clip_space = float3(input.tex_coord * 2.0 - 1.0, 1.0);
    float4 clip_pos = mul(camera->inv_projection_view_mat, float4(clip_space, 1.0));
    float3 world_pos = clip_pos.xyz / clip_pos.w;
    float3 direction = normalize(world_pos);

    float l = asin(direction.y);
    float u = atan2(direction.z, direction.x) / (2.0 * PI);
    float v = 0.5 - 0.5 * sign(l) * sqrt(abs(l) / (0.5 * PI));

    float3 color = c.sky_view_lut.sample(float2(u, v)).rgb;

    float3 eye_pos = world_pos * CAMERA_SCALE_UNIT + float3(0.0, atmos->planet_radius, 0.0);
    float h = length(eye_pos);
    float3 up_vec = eye_pos / h;
    
    float cos_theta = dot(sun.direction, up_vec);
    float2 transmittance_uv = get_transmittance_lut_uv(atmos, float2(h, cos_theta));
    float3 sun_transmittance = c.transmittance_lut.sample(c.world->linear_sampler, transmittance_uv).rgb;

    float _ = 0.0;
    if (!ray_hit_sphere(eye_pos, direction, atmos->planet_radius, _)) {
        color += draw_sun(direction, sun->direction, 1.0) * sun.intensity * sun_transmittance;
    }

    return float4(color, 1.0);
}


