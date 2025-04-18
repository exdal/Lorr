module imgui;

import std;
import gpu;

[[vk::binding(0, 0)]]
Sampler sampler;
[[vk::binding(1, 0)]]
Image2D<f32x4> texture;

struct VertexInput {
    f32x2 position  : POSITION;
    f32x2 tex_coord : TEXCOORD;
    f32x4 color     : COLOR;
};

struct VertexOutput {
    f32x4 position     : SV_Position;
    f32x2 tex_coord    : TEXCOORD;
    f32x4 color        : COLOR;
};

struct PushConstants {
    f32x2 translate;
    f32x2 scale;
    u32 is_srgb;
};
[[vk::push_constant]] PushConstants c;

[[shader("vertex")]]
func vs_main(VertexInput input) -> VertexOutput {
    VertexOutput output;

    output.position = f32x4(input.position * c.scale + c.translate, 0.0, 1.0);
    output.tex_coord = input.tex_coord;
    output.color = input.color;

    return output;
}

[[shader("fragment")]]
f32x4 fs_main(VertexOutput input) : SV_TARGET {
    f32x4 color_sampled = texture.sample_mip(sampler, input.tex_coord, 0.0) * input.color;
    f32x3 color = color_sampled.rgb;

    if (c.is_srgb == 0) {
        color = std::srgb_to_linear(color);
    }

    return f32x4(color, color_sampled.a);
}


