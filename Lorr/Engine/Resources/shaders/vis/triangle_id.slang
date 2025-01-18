module triangle_id;

import "../lorr";

struct VertexInput {
    float3 position  : POSITION;
    float3 normal    : NORMAL;
    float2 tex_coord : TEXCOORD;
    float4 color     : COLOR;
};

struct PushConstants {
    World *world;
    u32 model_index;
    u32 pad;
};
[[vk::push_constant]] PushConstants c;

struct VertexOutput {
    float4 position : SV_Position;
};

[shader("vertex")]
VertexOutput vs_main(VertexInput vertex, u32 vertex_id : SV_VertexID) {
    World *world = c.world;
    Camera *camera = &world->cameras[world.active_camera_index];
    Model *model = &world->models[c.model_index];

    VertexOutput output;
    let world_position = mul(float4(vertex.position, 1.0), model.model_transform_mat);
    output.position = mul(camera.projection_view_mat, world_position);
    return output;
}

#define RED_SEED 3
#define GREEN_SEED 5
#define BLUE_SEED 7

float IDToColor(uint ID, uint seed)
{
    return float(ID % seed) / float(seed);
}

float IDToColor(uint ID)
{
    return IDToColor(ID, RED_SEED);
}

float2 IDToColor2(uint ID)
{
    float2 color = float2(0, 0);
    color.x = IDToColor(ID, RED_SEED);
    color.y = IDToColor(ID, GREEN_SEED);

    return color;
}

float3 IDToColor3(uint ID)
{
    float3 color = float3(0, 0, 0);
    color.x = IDToColor(ID, RED_SEED);
    color.y = IDToColor(ID, GREEN_SEED);
    color.z = IDToColor(ID, BLUE_SEED);

    return color;
}


[shader("fragment")]
float4 fs_main(VertexOutput input, u32 primitive_id : SV_PrimitiveID) : SV_Target {
    float3 result = IDToColor3(primitive_id);
    return float4(result, 1.0);
}

