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

struct _Constants
{
    float2 Scale; 
    float2 Translate;
};

[[vk::push_constant]] ConstantBuffer<_Constants> Constants : register(b0, space2);

PixelInput main(VertexInput input)
{
    PixelInput output;

    output.Position = float4(input.Position * Constants.Scale + Constants.Translate, 0.0, 1.0);
    output.TexCoord = input.TexCoord;
    output.Color = input.Color;

    return output;
}