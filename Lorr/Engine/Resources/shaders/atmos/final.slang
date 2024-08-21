import "../lorr";

struct VertexOutput {
    float4 position     : SV_Position;
    float2 tex_coord    : TEXCOORD;
};

[[vk::push_constant]] struct {
    lr::World *world;
    ImageViewID sky_lut_image_id;
    SamplerID sky_lut_sampler_id;
} c;

[shader("vertex")]
VertexOutput vs_main(u32 vertex_id : SV_VertexID) {
    VertexOutput output;

    output.tex_coord = float2((vertex_id << 1) & 2, vertex_id & 2);
    output.position = float4(output.tex_coord * float2(2.0, 2.0) - float2(1.0, 1.0), 0.5, 1);

    return output;
}

float3 ACES(float3 x)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;

    return saturate(pow(abs((x * (a * x + b)) / (x * (c * x + d) + e)), 1 / 1.7));
}

float3 dither(float2 seed, float3 color)
{
    color *= 255;

    float rand = frac(sin(dot(seed, float2(12.9898, 78.233) * 2.0)) * 43758.5453);
    color = select(rand.xxx < (color - floor(color)), ceil(color), floor(color));
    color /= 255;

    return color;
}

[shader("fragment")]
float4 fs_main(VertexOutput input) : SV_TARGET
{
    const Texture2D sky_lut = Texture2D<float4>::get(c.sky_lut_image_id);
    const SamplerState sampler = SamplerState::get(c.sky_lut_sampler_id);
    
    float4 clip_space = float4(2.0 * input.tex_coord - 1.0, 1.0, 1.0);
    float4 world_space = mul(c.world->cameras[0].inv_projection_view_mat, clip_space);
    float3 world_pos = world_space.xyz / world_space.w;
    float3 direction = normalize(world_pos);

    float l = asin(direction.y);
    float u = atan2(direction.z, direction.x) / (2.0 * PI);
    float v = 0.5 - 0.5 * sign(l) * sqrt(abs(l) / (0.5 * PI));

    float3 color = sky_lut.Sample(sampler, float2(u, v)).rgb;
    color = ACES(color);
    color = dither(input.tex_coord, color);

    return float4(color, 1.0);
}


