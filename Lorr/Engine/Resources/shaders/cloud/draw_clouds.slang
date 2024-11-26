import cloud;
import "../atmos/atmos";
import "../lorr";

[[vk::push_constant]] struct {
    World *world;
    ImageView<float4> transmittance_lut_image;
    ImageView<float4> aerial_perspective_lut_image;
    SampledImage<float4> shape_noise_image;
    SampledImage<float4> detail_noise_image;
} c;

struct VertexOutput {
    vec4 position     : SV_Position;
    vec2 tex_coord    : TEXCOORD;
};

[shader("vertex")]
VertexOutput vs_main(u32 vertex_id : SV_VertexID) {
    VertexOutput output;

    output.tex_coord = vec2((vertex_id << 1) & 2, vertex_id & 2);
    output.position = vec4(output.tex_coord * 2.0 - 1.0, 0.5, 1);

    return output;
}

[shader("fragment")]
float4 fs_main(VertexOutput input) : SV_TARGET {
    World *world = c.world;
    Camera *camera = &world->cameras[world->active_camera_index];
    Clouds *clouds = &world->clouds;
    Atmosphere *atmos = &world->atmosphere;

    float3 clip_space = float3(input.tex_coord * 2.0 - 1.0, 1.0);
    float4 clip_pos = mul(camera->inv_projection_view_mat, float4(clip_space, 1.0));
    float3 clip_dir = clip_pos.xyz / clip_pos.w;
    float3 world_dir = normalize(clip_dir - camera->position);
    float3 world_pos = camera->position * CAMERA_SCALE_UNIT + float3(0.0, atmos->planet_radius, 0.0);

    CloudIntegrateInfo info = {};
    info.camera = camera;
    info.uv = input.tex_coord;
    info.sample_offset = 0.0;
    info.eye_pos = world_pos;
    info.eye_dir = world_dir;
    info.cloud_shape_image = c.shape_noise_image;
    info.cloud_detail_image = c.detail_noise_image;
    info.transmittance_lut_image = SampledImage<float4>(c.transmittance_lut_image.id(), world->linear_sampler);
    info.aerial_perspective_lut_image = SampledImage<float4>(c.aerial_perspective_lut_image.id(), world->linear_sampler);

    const let result = integrate_clouds(world, info);

    return float4(result.luminance, result.transmittance);
}
