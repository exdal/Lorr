import atmos;
import "../lorr";

[[vk::binding(0, 0)]] SamplerState sampler;
[[vk::binding(1, 0)]] Texture2D<float4> depth_image;
[[vk::binding(2, 0)]] Texture3D<float4> aerial_perspective_lut;
[[vk::binding(3, 0)]] Scene scene;

struct VertexOutput {
    float4 position     : SV_Position;
    float2 tex_coord    : TEXCOORD;
};

[shader("vertex")]
VertexOutput vs_main(u32 vertex_id : SV_VertexID) {
    VertexOutput output;
    output.tex_coord = float2((vertex_id << 1) & 2, vertex_id & 2);
    output.position = float4(output.tex_coord * 2.0 - 1.0, 1.0, 1.0);

    return output;
}

[shader("fragment")]
float4 fs_main(VertexOutput input) : SV_TARGET {
    Camera *active_camera = &scene.cameras[scene.active_camera_index];
    Atmosphere *atmos = scene.atmosphere;

    float sampled_depth = depth_image.Sample(sampler, input.tex_coord).r;
    if (sampled_depth == 1.0) {
        discard;
    }

    float3 clip_space = float3(input.tex_coord * 2.0 - 1.0, sampled_depth);
    float4 depth_position = mul(active_camera->inv_projection_view_mat, float4(clip_space, 1.0));
    float3 depth_world_position = depth_position.xyz / depth_position.w;
    float3 camera_relative_pos = (depth_world_position - active_camera.position) * CAMERA_SCALE_UNIT;
    float relative_depth = length(camera_relative_pos);

    float slice = relative_depth * (1.0 / atmos->aerial_km_per_slice);
    float weight = 1.0;
    if (slice < 0.5) {
        weight = saturate(slice * 2.0);
        slice = 0.5;
    }

    float w = sqrt(slice / 32.0); // from linear space to squared distribution
    float4 ap = weight * aerial_perspective_lut.Sample(sampler, float3(input.tex_coord, w));
    float3 luminance = ap.rgb;
    float transmittance = ap.a;

    return float4(luminance, transmittance);
}
