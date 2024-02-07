#pragma stage(pixel)

#include <lorr.hlsl>

struct PixelInput
{
    float4 position  : SV_POSITION;
    float2 tex_coord : TEXCOORD;
    float4 color     : COLOR;
};

[[vk::push_constant]] struct PushConstants
{
    Descriptors<2> descriptors;
} c;

float4 main(PixelInput input) : SV_TARGET
{
    const Texture2D font_texture = c.descriptors.get_texture(0);
    const SamplerState linear_sampler = c.descriptors.get_sampler(1);

    return /*font_texture.Sample(linear_sampler, input.tex_coord) **/ input.color;
}

