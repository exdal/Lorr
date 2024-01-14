#pragma stage(vertex)

#include <lorr.hlsl>

struct PixelInput
{
    float4 Position : SV_POSITION;
    float4 Color    : COLOR;
};

PixelInput main(uint vertex_id : SV_VertexID)
{
    PixelInput output;

    if(vertex_id == 0){
        output.Position = float4(0.0, -0.3, 0.0, 1.0);
        output.Color = float4(0, 1, 0, 1);
    } else if (vertex_id == 1){
        output.Position = float4(-0.3, 0.3, 0.0, 1.0);
        output.Color = float4(1, 0, 0, 1);
    } else {
        output.Position = float4(0.3, 0.3, 0.0, 1.0);
        output.Color = float4(0, 0, 1, 1);
    }

    return output;
}