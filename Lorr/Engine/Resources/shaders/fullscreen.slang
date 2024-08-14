import "lorr";

struct VertexOutput {
    float4 position     : SV_Position;
    float2 tex_coord    : TEXCOORD;
};

[[vk::push_constant]] struct
{
    ImageViewID image_view_id;
    SamplerID sampler_id;
} c;

[shader("vertex")]
VertexOutput vs_main(u32 vertex_id : SV_VertexID) {
    VertexOutput output;

    output.tex_coord =  float2((vertex_id << 1) & 2, vertex_id & 2);
    output.position = float4(output.tex_coord * float2(2, -2) + float2(-1, 1), 0.5, 1);

    return output;
}

[shader("fragment")]
float4 fs_main(VertexOutput input) : SV_TARGET
{
    const Texture2D font_texture = Texture2D<float4>::get(c.image_view_id);
    const SamplerState linear_sampler = SamplerState::get(c.sampler_id);

    return font_texture.Sample(linear_sampler, input.tex_coord);
}


