#version 460 core

#pragma $extensions
#pragma $stage_fragment

struct PixelInput
{
    vec2 TexCoord;
    vec4 Color;
};

layout(location = 0) in PixelInput pInput;
layout(location = 0) out vec4 fragColor;

void main()
{
    fragColor = pInput.Color;
}