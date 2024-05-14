import lorr;

[[vk::push_constant]] struct {
    ImageViewID image_view_id;
    float time;
} c;

[shader("compute")]
[numthreads(1, 1, 1)]
void cs_main(uint3 thread_id : SV_DispatchThreadID)
{
    const RWTexture2D tex = RWTexture2D<float4>::get(c.image_view_id);
    float2 uv = float2(thread_id.x / 128.0f, thread_id.y / 128.0f);
    float3 color = 0.5 + 0.5 * cos(c.time + uv.xyx + float3(0, 2, 4));
    tex[thread_id.xy] = float4(color, 1.0);
}
