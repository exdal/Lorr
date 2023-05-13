#version 460 core

struct PixelInput 
{
    vec2 TexCoord;
    vec4 Color;
};

layout(location = 0) in PixelInput pInput;
layout(location = 0) out vec4 fragColor;

layout(binding = 0, set = 0) uniform sampler u_Sampler[];
layout(binding = 1, set = 0) uniform texture2D u_Texture[];
layout(binding = 2, set = 0) uniform buffer u_Texture[];
layout(scalar, binding = 5, set = 0) uniform readonly buffer __u_DeviceAddressTable { uint64_t address[] } u_DeviceAddressTable;

void main()
{
    fragColor = texture(sampler2D(u_Texture, u_Sampler), pInput.TexCoord) * pInput.Color;
}