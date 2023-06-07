#version 460

#pragma $extensions
#pragma $stage_fragment

#include "shaders/common.glsl"

LR_INIT_SHADER();

struct PixelInput
{
    vec2 TexCoord;
    vec4 Color;
};

layout(location = 0) in PixelInput pInput;
layout(location = 0) out vec4 fragColor;

void main()
{
    fragColor = texture(sampler2D(LR_GET_IMAGE(1), LR_GET_SAMPLER(2)), pInput.TexCoord) * pInput.Color;
}