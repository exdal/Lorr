module imgui;
import lorr;

[[vk::binding(0, 0)]] SamplerState sampler;
[[vk::binding(1, 0)]] Texture2D<float4> texture;

struct VertexInput
{
    float2 position  : POSITION;
    float2 tex_coord : TEXCOORD;
    float4 color     : COLOR;
};

struct VertexOutput {
    float4 position     : SV_Position;
    float2 tex_coord    : TEXCOORD;
    float4 color        : COLOR;
};

struct PushConstants {
    float2 translate;
    float2 scale;
};
[[vk::push_constant]] PushConstants c;

[shader("vertex")]
VertexOutput vs_main(VertexInput input) {
    VertexOutput output;

    output.position = float4(input.position * c.scale + c.translate, 0.0, 1.0);
    output.tex_coord = input.tex_coord;
    output.color = input.color;

    return output;
}

[shader("fragment")]
float4 fs_main(VertexOutput input) : SV_TARGET {
    float4 color_sampled = texture.Sample(sampler, input.tex_coord) * input.color;
    return float4(lr::color_sRGB_EOTF(color_sampled.rgb), color_sampled.a);
}

