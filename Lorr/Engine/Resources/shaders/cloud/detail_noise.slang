import cloud;
import "../lorr";

// https://www.shadertoy.com/view/4fX3D8
float alligator(float3 position, float frequency, uint3 seed) {
    position *= frequency;
    float3 integer_coords = floor(position);
    float3 fractional_coords = position - integer_coords;

    float most_dense = 0.0;
    float most_dense_2 = 0.0;

    for (float ix = -1.0; ix <= 1.0; ix += 1.0) {
        for (float iy = -1.0; iy <= 1.0; iy += 1.0) {
            for (float iz = -1.0; iz <= 1.0; iz += 1.0) {
                float3 offset = float3(ix, iy, iz);
                float3 cell = integer_coords + offset;
                cell = fmod(cell, frequency);
                cell += float3(seed);
                float3 center = hash33(cell) + offset;
                float dist = distance(fractional_coords, center);
                float density = hash13(cell) * smoothstep_unchecked(1.0 - dist);
                if (most_dense < density) {
                    most_dense_2 = most_dense;
                    most_dense = density;
                } else if (most_dense_2 < density) {
                    most_dense_2 = density;
                }
            }
        }
    }

    return most_dense - most_dense_2;
}

float alligator(float3 position, float frequency, int octaves, float lacunarity, float persistence) {
    float amplitude = 1.0;
    float amplitudeSum = 0.0;
    float result = 0.0;
    uint3 seed = uint3(427);
    for(int i = 0; i < octaves; ++i) {
        result += alligator(position, frequency, seed) * amplitude;
        amplitudeSum += amplitude;
        frequency *= lacunarity;
        amplitude *= persistence;
        seed += uint3(frequency);
    }
    result /= amplitudeSum;

    return result;
}

float alligator(float3 pos, float f, int o, float l) {
    float gain = 0.5;
    float noise = alligator(pos, f, o, l, gain);
    noise = clamp(noise * 1.5, 0.0, 1.0);
    noise = pow(noise, 0.7);
    return noise;
}

[[vk::push_constant]] struct {
    ImageView detail_image_view;
    float3 image_size;
} c;

[shader("compute")]
[numthreads(16, 16, 1)]
void cs_main(uint3 thread_id : SV_DispatchThreadID) {
    float3 uvw = (float3(thread_id.xyz) + 0.5) / c.image_size;

    float noise_hf = alligator(uvw, 3.0, 6, 2.122);
    float noise_lf = alligator(uvw, 6.0, 8, 2.2);
    c.detail_image_view.store(thread_id, float4(noise_hf, noise_lf, 0.0, 1.0));
}
