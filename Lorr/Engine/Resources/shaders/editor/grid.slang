import "../lorr";

struct VertexOutput {
    float4 position     : SV_Position;
    float2 tex_coord    : TEXCOORD;
};

[shader("vertex")]
VertexOutput vs_main(u32 vertex_id : SV_VertexID) {
    VertexOutput output;

    output.tex_coord =  float2((vertex_id << 1) & 2, vertex_id & 2);
    output.position = float4(output.tex_coord * float2(2.0, 2.0) - float2(1.0, 1.0), 0.5, 1);

    return output;
}

[shader("fragment")]
float4 fs_main(VertexOutput input) : SV_TARGET
{
    return float4(input.tex_coord.xy, 0.0, 1.0);
}

