module imgui;

import lorr;

#define GAMMA_TO_LINEAR(Gamma) ((Gamma) < 0.04045 ? (Gamma) / 12.92 : pow(max((Gamma) + 0.055, 0.0) / 1.055, 2.4))
#define SRGBA_TO_LINEAR(col) \
    col.r = GAMMA_TO_LINEAR(col.r); \
    col.g = GAMMA_TO_LINEAR(col.g); \
    col.b = GAMMA_TO_LINEAR(col.b); \
    col.a = 1.0 - GAMMA_TO_LINEAR(1.0 - col.a);

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

[[vk::push_constant]] struct
{
    float2 translate;
    float2 scale;
    SampledImage<float4> image;
} c;

[shader("vertex")]
VertexOutput vs_main(VertexInput input) {
    VertexOutput output;

    output.position = float4(input.position * c.scale + c.translate, 0.0, 1.0);
    output.tex_coord = input.tex_coord;
    output.color = input.color;
    SRGBA_TO_LINEAR(output.color);

    return output;
}

[shader("fragment")]
float4 fs_main(VertexOutput input) : SV_TARGET {
    float4 col =  c.image.sample(input.tex_coord) * input.color;
    col.rgb *= col.a;

    return col;
}

