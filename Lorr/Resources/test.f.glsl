#version 450

layout(location = 0) out vec4 o_Color;

layout(set = 0, binding = 0) uniform _FragInfo
{
    vec4 Color;
} FragInfo;

void main()
{
    o_Color = FragInfo.Color;
}