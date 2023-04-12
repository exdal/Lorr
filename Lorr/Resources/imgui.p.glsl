#version 460 core

struct PixelInput 
{
    vec2 TexCoord;
    vec4 Color;
};

layout(location = 0) in PixelInput pInput;
layout(location = 0) out vec4 fragColor;

layout(set = 0, binding = 0) uniform texture2D u_Texture;
layout(set = 1, binding = 0) uniform sampler u_Sampler;

void main()
{
    fragColor = texture(sampler2D(u_Texture, u_Sampler), pInput.TexCoord) * pInput.Color;
}