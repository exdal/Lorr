module tonemap;
import lorr;

[[vk::binding(0, 0)]] Texture2D<float4> input_image;
[[vk::binding(1, 0)]] RWTexture2D<float4> output_image;
[[vk::push_constant]] uint3 image_size;

float3 ACES_Film(float3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return (x * (a * x + b)) / (x * (c * x + d) + e);
}

static const float3x3 ACESInputMat = {
    { 0.59719, 0.35458, 0.04823 },
    { 0.07600, 0.90834, 0.01566 },
    { 0.02840, 0.13383, 0.83777 }
};

// ODT_SAT => XYZ => D60_2_D65 => sRGB
static const float3x3 ACESOutputMat = {
    { 1.60475, -0.53108, -0.07367 },
    { -0.10208, 1.10813, -0.00605 },
    { -0.00327, -0.07276, 1.07602 }
};

float3 RRTAndODTFit(float3 v) {
    float3 a = v * (v + 0.0245786f) - 0.000090537f;
    float3 b = v * (0.983729f * v + 0.4329510f) + 0.238081f;
    return a / b;
}

// ACES Filmic (Fitted)
float3 ACES_Fitted(float3 color) {
    color = mul(ACESInputMat, color);

    // Apply RRT and ODT
    color = RRTAndODTFit(color);

    color = mul(ACESOutputMat, color);

    // Clamp to [0, 1]
    color = saturate(color);

    return color;
}

float3 PBRNeutralToneMapping(float3 color) {
    const float startCompression = 0.8 - 0.04;
    const float desaturation = 0.15;

    float x = min(color.r, min(color.g, color.b));
    float offset = x < 0.08 ? x - 6.25 * x * x : 0.04;
    color -= offset;

    float peak = max(color.r, max(color.g, color.b));
    if (peak < startCompression) return color;

    const float d = 1.0 - startCompression;
    float newPeak = 1.0 - d * d / (peak + d - startCompression);
    color *= newPeak / peak;

    float g = 1.0 - 1.0 / (desaturation * (peak - newPeak) + 1.0);
    return lerp(color, newPeak, g);
}

[shader("compute")]
[numthreads(8, 8, 1)]
void cs_main(uint3 thread_id : SV_DispatchThreadID) {
    if (any(thread_id >= image_size)) {
        return;
    }

    float3 color = input_image.Load(thread_id).rgb;
    //color = ACES_Film(color);
    //color = ACES_Fitted(color);
    color = PBRNeutralToneMapping(color);  // this looks like shit, figure out why
    color = lr::color_sRGB_OETF(color);
    output_image[thread_id.xy] = float4(color, 1.0);
}

