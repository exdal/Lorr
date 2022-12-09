struct VertexInput 
{
    float2 Position : POSITION;
    float2 TexCoord : TEXCOORD;
    float4 Color    : COLOR;
};

struct PixelInput
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD;
    float4 Color    : COLOR;
};

struct _CameraInfo
{
    matrix VPMatrix;
};

[[vk::push_constant]] ConstantBuffer<_CameraInfo> CameraInfo : register(b0, space2);

PixelInput main(VertexInput input)
{
    PixelInput output;

    output.Position = mul(CameraInfo.VPMatrix, float4(input.Position, 0.0, 1.0));
    output.TexCoord = input.TexCoord;
    output.Color = input.Color;

    return output;
}