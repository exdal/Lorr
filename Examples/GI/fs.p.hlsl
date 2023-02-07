[[vk::binding(0, 0)]] Texture2D<float4> u_Texture : register(t0, space0);
[[vk::binding(0, 1)]] SamplerState u_Sampler      : register(s0, space1);

struct PSInput
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD;
};

float3 ACES(float3 x)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;

    return abs((x * (a * x + b)) / (x * (c * x + d) + e));
}

float4 main(PSInput input) : SV_TARGET
{
    float3 color = u_Texture.Sample(u_Sampler, input.TexCoord).xyz;
    color = saturate(pow(ACES(color), 1.0 / 2.2));

    return float4(color, 1.0); 
}