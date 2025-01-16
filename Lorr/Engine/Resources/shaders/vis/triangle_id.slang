module triangle_id;

import "../lorr";

struct PushConstants {
    World *world;
    u32 model_index;
    u32 material_index;
};
[[vk::push_constant]] PushConstants c;

struct VertexInput {
    float3 position  : POSITION;
    float3 normal    : NORMAL;
    float2 tex_coord : TEXCOORD;
    u32 color        : COLOR;
};

struct VertexOutput {
    float4 position  : SV_Position;
    float2 tex_coord : TEXCOORD;
};

[shader("vertex")]
VertexOutput vs_main(VertexInput input) {
    World *world = c.world;
    Camera *camera = &world->cameras[world.active_camera_index];
    Model *model = &world->models[c.model_index];

    VertexOutput output;
    let world_position = mul(float4(input.position, 1.0), model.model_transform_mat);
    output.position = mul(camera.projection_view_mat, world_position);
    output.tex_coord = input.tex_coord;
    return output;
}

[shader("fragment")]
float4 fs_main(VertexOutput input) : SV_Target {
    World *world = c.world;
    Material *material = &world->materials[c.material_index];
    float3 albedo_color = material->albedo_image.sample(input.tex_coord).rgb;

    return float4(albedo_color, 1.0);
}

