import cloud;
import "../lorr";

[[vk::binding(0, 0)]] RWTexture3D<float4> shape_noise_lut;
[[vk::binding(1, 0)]] RWTexture3D<float4> detail_noise_lut;
[[vk::binding(2, 0)]] ConstantBuffer<Scene, ScalarDataLayout> scene;

// WARN: These hashes are extremely fragile. Replacing one of them would collapse all noise functions.
public float hash13(float3 p) {
    const static uint3 VPRIME = uint3(3480082861U, 2420690917U, 2149110343U);
    const static uint FPRIME = 1317666547U;
    const static float UIF = 1.0 / float(0xffffffffU);

    uint3 q = uint3(int3(p)) * VPRIME;
    uint n = (q.x & q.y ^ q.z) * FPRIME;
    return float(n) * UIF;
}

public float3 hash33(float3 p) {
    const static uint UI0 = 1597334673U;
    const static uint UI1 = 3812015801U;
    const static uint2 UI2 = uint2(UI0, UI1);
    const static uint3 UI3 = uint3(UI0, UI1, 2798796415U);
    const static float UIF = 1.0 / float(0xffffffffU);

    uint3 q = uint3(int3(p)) * UI3;
    q = (q.x ^ q.y ^ q.z) * UI3;
    return float3(q) * UIF;
}

float smoothstep_unchecked(float x) {
    x = clamp(x, 0.0, 1.0);
    return x * x * (3.0 - 2.0 * x);
}

// FBM noise by iq
float gradient_noise(float3 x, float freq) {
    float3 p = floor(x);
    float3 w = frac(x);
    float3 u = w * w * w * (w * (w * 6. - 15.) + 10.);

    float3 ga = -1.0 + 2.0 * hash33(lr::mod(p + float3(0., 0., 0.), freq));
    float3 gb = -1.0 + 2.0 * hash33(lr::mod(p + float3(1., 0., 0.), freq));
    float3 gc = -1.0 + 2.0 * hash33(lr::mod(p + float3(0., 1., 0.), freq));
    float3 gd = -1.0 + 2.0 * hash33(lr::mod(p + float3(1., 1., 0.), freq));
    float3 ge = -1.0 + 2.0 * hash33(lr::mod(p + float3(0., 0., 1.), freq));
    float3 gf = -1.0 + 2.0 * hash33(lr::mod(p + float3(1., 0., 1.), freq));
    float3 gg = -1.0 + 2.0 * hash33(lr::mod(p + float3(0., 1., 1.), freq));
    float3 gh = -1.0 + 2.0 * hash33(lr::mod(p + float3(1., 1., 1.), freq));

    float va = dot(ga, w - float3(0., 0., 0.));
    float vb = dot(gb, w - float3(1., 0., 0.));
    float vc = dot(gc, w - float3(0., 1., 0.));
    float vd = dot(gd, w - float3(1., 1., 0.));
    float ve = dot(ge, w - float3(0., 0., 1.));
    float vf = dot(gf, w - float3(1., 0., 1.));
    float vg = dot(gg, w - float3(0., 1., 1.));
    float vh = dot(gh, w - float3(1., 1., 1.));

    return va +
           u.x * (vb - va) +
           u.y * (vc - va) +
           u.z * (ve - va) +
           u.x * u.y * (va - vb - vc + vd) +
           u.y * u.z * (va - vc - ve + vg) +
           u.z * u.x * (va - vb - ve + vf) +
           u.x * u.y * u.z * (-va + vb + vc - vd + ve - vf - vg + vh);
}

float perlin_fbm(float3 p, float freq, int octaves) {
    const float H = 0.85f;
    float G = exp2(-H);
    float amp = 1.0f;
    float noise = 0.0f;
    for (int i = 0; i < octaves; i++) {
        noise += amp * gradient_noise(p * freq, freq);
        freq *= 2.0;
        amp *= G;
    }

    return noise;
}

float alligator(float3 position, int grid_size, float3 seed) {
    position *= grid_size;
    
    float3 id = floor(position);
    float3 grid = position - id;

    // Priority queue
    float d1 = 0.0;
    float d2 = 0.0;
    for (int ix = -1; ix <= 1; ++ix) {
        for (int iy = -1; iy <= 1; ++iy) {
            for (int iz = -1; iz <= 1; ++iz) {
                float3 offset = float3(ix, iy, iz);
                float3 cell = id + offset;
                cell = lr::mod(cell, float(grid_size));
                cell += seed;

                float3 center = hash33(cell) + offset;
                float dist = distance(grid, center);
                float density = hash13(cell) * smoothstep_unchecked(1.0 - dist);
                if (density < 1.0) {
                    if (d1 < density) {
                        d2 = d1;
                        d1 = density;
                    } else if (d2 < density) {
                        d2 = density;
                    }
                }
            }
        }
    }

    return d1 - d2;
}

float alligator_fbm(float3 position, float frequency, int octaves, float lacunarity, float persistence) {
    float amplitude = 1.0;
    float amplitude_sum = 0.0;
    float result = 0.0;
    float3 seed = float3(421.0);
    for (int i = 0; i < octaves; ++i) {
        result += alligator(position, uint(frequency), seed) * amplitude;
        amplitude_sum += amplitude;
        frequency *= lacunarity;
        amplitude *= persistence;
        seed += frequency;
    }
    result /= amplitude_sum;

    result = clamp(result * 2.4, 0.0, 1.0);
    result = pow(result, 0.6);
    return result;
}

float alligator_fbm_4(float3 position, float frequency) {
    return
        alligator_fbm(position, frequency * 1.0, 8, 2.200, 0.5) * 0.500 +
        alligator_fbm(position, frequency * 2.0, 8, 2.112, 0.5) * 0.250 +
        alligator_fbm(position, frequency * 4.0, 8, 2.233, 0.5) * 0.125 +
        alligator_fbm(position, frequency * 8.0, 8, 2.150, 0.5) * 0.0625;
}

[shader("compute")]
[numthreads(16, 16, 1)]
void cs_main(uint3 thread_id : SV_DispatchThreadID) {
    float3 uvw = (float3(thread_id) + 0.5) / float3(scene.clouds.noise_lut_size);

    const static float SHAPE_FREQUENCY = 1.0;
    float4 shape_color = float4(0.0);
    shape_color.x = remap(abs(
                        lerp(1.0, perlin_fbm(uvw, SHAPE_FREQUENCY * 1.0, 8), 0.5) * 2.0 - 1.0), 0.0,
                        1.0, alligator_fbm_4(uvw, SHAPE_FREQUENCY * 1.0), 1.0);
    shape_color.y = remap(abs(
                        lerp(1.0, perlin_fbm(uvw, SHAPE_FREQUENCY * 2.0, 8), 0.5) * 2.0 - 1.0), 0.0,
                        1.0, alligator_fbm_4(uvw, SHAPE_FREQUENCY * 2.0), 1.0);
    shape_color.z = remap(abs(
                        lerp(1.0, perlin_fbm(uvw, SHAPE_FREQUENCY * 3.0, 8), 0.5) * 2.0 - 1.0), 0.0,
                        1.0, alligator_fbm_4(uvw, SHAPE_FREQUENCY * 3.0), 1.0);
    shape_color.w = remap(abs(
                        lerp(1.0, perlin_fbm(uvw, SHAPE_FREQUENCY * 4.0, 8), 0.5) * 2.0 - 1.0), 0.0,
                        1.0, alligator_fbm_4(uvw, SHAPE_FREQUENCY * 4.0), 1.0);
    shape_noise_lut[thread_id] = shape_color;

    const static float DETAIL_FREQUENCY = 3.0;
    float4 detail_color = float4(0.0);
    detail_color.x = alligator_fbm_4(uvw, DETAIL_FREQUENCY * 1.0);
    detail_color.y = alligator_fbm_4(uvw, DETAIL_FREQUENCY * 2.0);
    detail_color.z = alligator_fbm_4(uvw, DETAIL_FREQUENCY * 4.0);
    detail_color.w = alligator_fbm_4(uvw, DETAIL_FREQUENCY * 8.0);
    detail_noise_lut[thread_id] = detail_color;
}

