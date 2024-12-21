import cloud;
import "../lorr";

struct PushConstants {
    float3 image_size_inv;
    ImageView<float4> noise_image;
};
[[vk::push_constant]] PushConstants c;

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
    
    float densest = -3.402823466e+38F;
    float second_densest = -3.402823466e+38F;
    
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
                if (densest < density) {
                    second_densest = densest;
                    densest = density;
                } else if (second_densest < density) {
                    second_densest = density;
                }
            }
        }
    }
    return densest - second_densest;
}

float alligator(float3 position, float frequency, int octaves, float lacunarity, float persistence) {
    float amplitude = 1.0;
    float amplitude_sum = 0.0;
    float result = 0.0;
    float3 seed = float3(1582.0);
    for (int i = 0; i < octaves; ++i) {
        result += alligator(position, uint(frequency), seed) * amplitude;
        amplitude_sum += amplitude;
        frequency *= lacunarity;
        amplitude *= persistence;
        seed += frequency;
    }
    result /= amplitude_sum;

    result *= 1.8;
    result = pow(result, 0.7);
    return result;
}

float alligator_fbm(float3 position, float frequency)
{
    return alligator(position, frequency * 1.0, 8, 2.200, 0.5) * 0.625 +
           alligator(position, frequency * 2.0, 8, 2.112, 0.5) * 0.250 +
           alligator(position, frequency * 4.0, 8, 2.233, 0.5) * 0.125;
}

[shader("compute")]
[numthreads(16, 16, 1)]
void shape_noise_cs_main(uint3 thread_id : SV_DispatchThreadID) {
    const float BASE_FREQUENCY = 2.0;
    float3 uvw = c.image_size_inv * ((float3)thread_id + 0.5f);
    float perlin = lerp(1.0, perlin_fbm(uvw, 3.0, 8), 0.5);
    perlin = abs(perlin * 2.0 - 1.0);  // billowy perlin noise

    float4 color = float4(0.0);
    color.y = alligator_fbm(uvw, BASE_FREQUENCY * 1.0);
    color.z = alligator_fbm(uvw, BASE_FREQUENCY * 2.0);
    color.w = alligator_fbm(uvw, BASE_FREQUENCY * 4.0);
    color.r = remap(perlin, 0.0, 1.0, color.y, 1.0);

    c.noise_image.store(thread_id, color);
}

[shader("compute")]
[numthreads(16, 16, 1)]
void detail_noise_cs_main(uint3 thread_id : SV_DispatchThreadID) {
    const float BASE_FREQUENCY = 4.0;
    float3 uvw = c.image_size_inv * ((float3)thread_id + 0.5f);

    float4 color = float4(0.0);
    color.x = alligator_fbm(uvw, BASE_FREQUENCY * 1.0);
    color.y = alligator_fbm(uvw, BASE_FREQUENCY * 2.0);
    color.z = alligator_fbm(uvw, BASE_FREQUENCY * 4.0);

    float cloud_type = uvw.x;
    float height = uvw.y;
    const float4 strato_gradient = float4(0.0f, 0.07f, 0.08f, 0.15f);
    const float4 strato_cumulus_gradient = float4(0.0f, 0.2f, 0.42f, 0.6f);
    const float4 culumulus_gradient = float4(0.0f, 0.08f, 0.75f, 0.98f);
    float layer_a = 1.0f - saturate(cloud_type * 2.0f);
    float layer_b = 1.0f - abs(cloud_type - 0.5f) * 2.0f;
    float layer_c = saturate(cloud_type - 0.5f) * 2.0f;
    float4 gradient = strato_gradient * layer_a + strato_cumulus_gradient * layer_b + culumulus_gradient * layer_c;
    float v1 = saturate(lerp(height, gradient.x, gradient.y));
    float v2 = saturate(lerp(height, gradient.w, gradient.z));
    color.w = v1 * v2;

    c.noise_image.store(thread_id, color);
}

