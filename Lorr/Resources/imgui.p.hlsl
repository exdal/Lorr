struct PixelInput 
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD;
    float4 Color    : COLOR;
};

[[vk::binding(0, 0)]] Texture2D<float4> u_Texture : register(t0, space0);
[[vk::binding(0, 1)]] SamplerState u_Sampler      : register(s0, space1);

float4 main(PixelInput input) : SV_TARGET
{
    return u_Texture.SampleLevel(u_Sampler, input.TexCoord, 0.0) * input.Color;
}