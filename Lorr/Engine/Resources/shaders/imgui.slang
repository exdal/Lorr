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
    u32 is_srgb;
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

float3 SRGBToLinear(float3 color)
{
    // Approximately pow(color, 2.2)
	return select(color < 0.04045, color / 12.92, pow(abs(color + 0.055) / 1.055, 2.4));
}

float3 color_sRGB_EOTF(float3 srgb_nonlinear) {
    let cutoff = srgb_nonlinear < 0.04045;
    vec3 higher = pow((srgb_nonlinear + 0.055) / 1.055, 2.4);
    vec3 lower = srgb_nonlinear / 12.92;

    return lerp(higher, lower, float3(cutoff));
}

[shader("fragment")]
float4 fs_main(VertexOutput input) : SV_TARGET {
    float4 color_sampled = texture.Sample(sampler, input.tex_coord) * input.color;
    float3 color = color_sampled.rgb;

    if (c.is_srgb == 0) {
        //color = SRGBToLinear(color);
    }

    return float4(color, color_sampled.a);
}

