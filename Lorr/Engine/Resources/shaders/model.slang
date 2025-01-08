module model;

import lorr;
import atmos.atmos;

struct PushConstants {
    World *world;
    ImageView<float4> transmittance_lut;
    u32 model_transform_index;
    u32 material_index;
    u32 pad;
};
[[vk::push_constant]] PushConstants c;

struct VertexInput {
    float3 position  : POSITION;
    float uv_x       : UV_X;
    float3 normal    : NORMAL;
    float uv_y       : UV_Y;
};

struct VertexOutput {
    float4 position         : SV_Position;
    float4 world_position   : POSITION;
    float3 normal           : NORMAL;
    float2 tex_coord        : TEXCOORD;
};

[shader("vertex")]
VertexOutput vs_main(VertexInput input) {
    World *world = c.world;
    Camera *camera = &world->cameras[world.active_camera_index];
    Model *model = &world->models[c.model_transform_index];

    VertexOutput output;
    output.world_position = mul(float4(input.position, 1.0), model.model_transform_mat);
    output.position = mul(camera.projection_view_mat, output.world_position);
    output.normal = normalize(mul(float4(input.normal, 1.0), model.world_transform_mat).xyz);
    output.tex_coord = float2(input.uv_x, input.uv_y);
    return output;
}

[shader("fragment")]
float4 fs_main(VertexOutput input) : SV_Target {
    World *world = c.world;
    Atmosphere *atmos = world->atmosphere;
    Sun *sun = world->sun;
    Camera *camera = &world->cameras[world->active_camera_index];
    Material *material = &world->materials[c.material_index];

    float3 world_position = input.world_position.xyz * CAMERA_SCALE_UNIT + float3(0.0, atmos->planet_radius, 0.0);
    float h = length(world_position);
    float3 up_vec = world_position / h;
    float cos_theta = dot(sun->direction, up_vec);
    float2 transmittance_uv = get_transmittance_lut_uv(atmos, float2(h, cos_theta));
    float3 sun_transmittance = c.transmittance_lut.sample(c.world->linear_sampler, transmittance_uv).rgb;

    float3 albedo_color = material->albedo_image.sample(input.tex_coord).rgb;
    float diff = max(dot(input.normal, sun->direction), 0.0);
    float3 ambient = float3(0.2) * albedo_color;
    float3 diffuse = diff * albedo_color;

    return float4(diffuse * 0.5 * sun_transmittance, 1.0);
}
