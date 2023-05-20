#version 460 core

#pragma $extensions
#pragma $stage_vertex

struct PixelInput
{
    vec2 TexCoord;
    vec4 Color;
};

layout(location = 0) in vec2 vPosition;
layout(location = 1) in vec2 vTexCoord;
layout(location = 2) in vec4 vColor;

layout(location = 0) out PixelInput pOutput;

layout(push_constant) uniform _Constants
{
    vec2 Scale; 
    vec2 Translate;
} Constants;

out gl_PerVertex { vec4 gl_Position; };
void main()
{
    pOutput.TexCoord = vTexCoord;
    pOutput.Color = vColor;
    gl_Position = vec4(vPosition * Constants.Scale + Constants.Translate, 0.0, 1.0);
}