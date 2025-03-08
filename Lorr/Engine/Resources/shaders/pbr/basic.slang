import "../lorr";

[[vk::binding(0, 0)]] SamplerState sampler;
[[vk::binding(1, 0)]] Texture2D<float4> albedo_image;

struct VertexOutput {
    float4 position : SV_Position;
    float2 tex_coord : TEXCOORD;
};

[shader("vertex")]
VertexOutput vs_main(u32 vertex_id : SV_VertexID) {
    VertexOutput output;
    output.tex_coord = float2((vertex_id << 1) & 2, vertex_id & 2);
    output.position = float4(2.0 * output.tex_coord - 1.0, 1.0, 1.0);

    return output;
}

[shader("fragment")]
float4 fs_main(VertexOutput input) : SV_Target {
    float3 color = albedo_image.Sample(sampler, input.tex_coord).rgb;
    return float4(color, 1.0);
}

