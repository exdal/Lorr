import "../lorr";

float smoothstep_unchecked(float x) {
    x = clamp(x, 0.0, 1.0);
    return x * x * (3.0 - 2.0 * x);
}

float remap(float orig, float old_min, float old_max, float new_min, float new_max)
{
    return new_min + (((orig - old_min) / (old_max - old_min)) * (new_max - new_min));
}

#define FPRIME 1317666547U
#define VPRIME uint3(3480082861U, 2420690917U, 2149110343U)
#define SMALLESTFLOAT (1.0 / float(0xffffffffU))
float hash13(float3 p) {
    uint3 q = uint3(int3(p)) * VPRIME;
    uint n = (q.x & q.y ^ q.z) * FPRIME;
    return float(n) * SMALLESTFLOAT;
}

// 3 in 3 out hash from 
#define UI0 1597334673U
#define UI1 3812015801U
#define UI3 uint3(UI0, UI1, 2798796415U)
#define UIF (1.0 / float(0xffffffffU))
float3 hash33(float3 p) {
    uint3 q = uint3(int3(p)) * UI3;
    q = (q.x ^ q.y ^ q.z)*UI3;
    return float3(q) * UIF;
}

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

float alligator(float3 position, float frequency,  int octaves, float lacunarity, float persistence) {
    
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
