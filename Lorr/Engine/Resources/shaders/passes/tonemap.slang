module tonemap;

import std;
import gpu;

[[vk::binding(0, 0)]]
Sampler sampler;
[[vk::binding(1, 0)]]
Image2D<f32x4> input_image;

struct VertexOutput {
    f32x4 position : SV_Position;
    f32x2 tex_coord : TEXCOORD;
};

[[shader("vertex")]]
VertexOutput vs_main(u32 vertex_id : SV_VertexID) {
    VertexOutput output;
    output.tex_coord = f32x2((vertex_id << 1) & 2, vertex_id & 2);
    output.position = f32x4(2.0 * output.tex_coord - 1.0, 1.0, 1.0);

    return output;
}

f32x3 ACES_Film(f32x3 x) {
    const f32 a = 2.51;
    const f32 b = 0.03;
    const f32 c = 2.43;
    const f32 d = 0.59;
    const f32 e = 0.14;
    return (x * (a * x + b)) / (x * (c * x + d) + e);
}

static const f32x3x3 ACESInputMat = {
    { 0.59719, 0.35458, 0.04823 },
    { 0.07600, 0.90834, 0.01566 },
    { 0.02840, 0.13383, 0.83777 }
};

// ODT_SAT => XYZ => D60_2_D65 => sRGB
static const f32x3x3 ACESOutputMat = {
    { 1.60475, -0.53108, -0.07367 },
    { -0.10208, 1.10813, -0.00605 },
    { -0.00327, -0.07276, 1.07602 }
};

f32x3 RRTAndODTFit(f32x3 v) {
    f32x3 a = v * (v + 0.0245786f) - 0.000090537f;
    f32x3 b = v * (0.983729f * v + 0.4329510f) + 0.238081f;
    return a / b;
}

// ACES Filmic (Fitted)
f32x3 ACES_Fitted(f32x3 color) {
    color = mul(ACESInputMat, color);

    // Apply RRT and ODT
    color = RRTAndODTFit(color);

    color = mul(ACESOutputMat, color);

    // Clamp to [0, 1]
    color = saturate(color);

    return color;
}

f32x3 PBRNeutralToneMapping(f32x3 color) {
    const f32 startCompression = 0.8 - 0.04;
    const f32 desaturation = 0.15;

    f32 x = min(color.r, min(color.g, color.b));
    f32 offset = x < 0.08 ? x - 6.25 * x * x : 0.04;
    color -= offset;

    f32 peak = max(color.r, max(color.g, color.b));
    if (peak < startCompression) return color;

    const f32 d = 1.0 - startCompression;
    f32 newPeak = 1.0 - d * d / (peak + d - startCompression);
    color *= newPeak / peak;

    f32 g = 1.0 - 1.0 / (desaturation * (peak - newPeak) + 1.0);
    return lerp(color, newPeak, g);
}

[[shader("fragment")]]
f32x4 fs_main(VertexOutput input) {
    f32x3 color = input_image.sample_mip(sampler, input.tex_coord, 0.0).rgb;
    //color = ACES_Film(color);
    //color = ACES_Fitted(color);
    color = PBRNeutralToneMapping(color);  // this looks like shit, figure out why
    return f32x4(color, 1.0);
}

