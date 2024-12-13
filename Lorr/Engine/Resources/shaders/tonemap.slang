module tonemap;

import lorr;

float3 aces(float3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return (x * (a * x + b)) / (x * (c * x + d) + e);
}

[[vk::push_constant]] struct {
    World *world;
    ImageView<float4> src_image_view;
    f32 avg_luminance;
} c;

struct VertexOutput {
    float4 position     : SV_Position;
    float2 tex_coord    : TEXCOORD;
};

[shader("vertex")]
VertexOutput vs_main(u32 vertex_id : SV_VertexID) {
    VertexOutput output;
    output.tex_coord = float2((vertex_id << 1) & 2, vertex_id & 2);
    output.position = float4(output.tex_coord * 2.0 - 1.0, 1.0, 1.0);

    return output;
}

[shader("fragment")]
float4 fs_main(VertexOutput input) : SV_TARGET {
    World *world = c.world;
    float3 color = c.src_image_view.sample(world->linear_sampler, input.tex_coord).rgb;
    return float4(aces(color), 1.0);
}

