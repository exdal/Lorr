#pragma stage(vertex)

#include <lorr.hlsl>

struct VertexInput
{
    float2 position  : POSITION;
    float2 tex_coord : TEXCOORD;
    float4 color     : COLOR;
};

struct PixelInput
{
    float4 position  : SV_POSITION;
    float2 tex_coord : TEXCOORD;
    float4 color     : COLOR;
};

[[vk::push_constant]] struct PushConstants
{
    float2 scale;
    float2 translate;
} c;

PixelInput main(VertexInput input)
{
    PixelInput output;

    output.position = float4(input.position * c.scale + c.translate, 0.0, 1.0);
    output.tex_coord = input.tex_coord;
    output.color = input.color;

    return output;
}