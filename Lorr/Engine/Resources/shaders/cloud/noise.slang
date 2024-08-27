import "../lorr";
import "common";

[[vk::push_constant]] struct {
    lr::World *world;
    float2 image_size;
    ImageViewID target_image_id;
} c;

float alligator(float2 pos, float f, int o, float l) {
    float gain = 0.5;
    float noise = alligator(float3(pos, 0.5), f, o, l, gain);
    noise = clamp(noise * 1.5, 0.0, 1.0);
    noise = pow(noise, 0.7);
    return noise;
}

[shader("compute")]
[numthreads(16, 16, 1)]
void cs_main(uint3 thread_id : SV_DispatchThreadID) {
    RWTexture2D target_image = RWTexture2D<float4>::get(c.target_image_id);
    float2 uv = (float2(thread_id.xy) + 0.5) / c.image_size;

    float hfa = alligator(uv, 3.0, 6, 2.122);
    float lfa = alligator(uv, 6.0, 8, 2.2);
    target_image[thread_id.xy] = float4(
        lfa,
        hfa,
        float2(0.0)  // TODO: Add Curl-Alligator noises
    );
}

