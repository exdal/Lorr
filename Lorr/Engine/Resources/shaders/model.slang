module model;

import lorr;
import atmos.atmos;

[[vk::push_constant]] struct {
    World *world;
    ImageViewID transmittance_lut;
    u32 model_transform_index;
    u32 material_id;
    u32 pad;
} c;

struct VertexInput {
    float3 position  : POSITION;
    float uv_x       : UV_X;
    float3 normal    : NORMAL;
    float uv_y       : UV_Y;
};

struct VertexOutput {
    float4 position         : SV_Position;
    float4 world_position   : POSITION;
    float3 normal           : NORMAL;
    float2 tex_coord        : TEXCOORD;
};

[shader("vertex")]
VertexOutput vs_main(VertexInput input) {
    const let world = c.world;
    const let camera = world.cameras[world.active_camera_index];
    const let model = world.models[c.model_transform_index];

    VertexOutput output;
    output.world_position = mul(float4(input.position, 1.0), model.model_transform_mat);
    output.position = mul(camera.projection_view_mat, output.world_position);
    output.normal = normalize(mul(float4(input.normal, 1.0), model.world_transform_mat).xyz);
    output.tex_coord = float2(input.uv_x, input.uv_y);
    return output;
}

[shader("fragment")]
float4 fs_main(VertexOutput input) : SV_Target {
    World *world = c.world;
    Atmosphere *atmos = &world->atmosphere;
    const let transmittance_image = ImageView(c.transmittance_lut);
    
    Camera active_camera = world->cameras[world->active_camera_index];
    
    Material material = world->materials[c.material_id];
    const let texture = ImageView(material.albedo_image_view);
    float3 sampled_albedo_color = texture.sample(material.albedo_sampler, input.tex_coord).rgb;
    const let albedo_color = material.albedo_color.xyz;

    const float scaling_unit = 0.1;  // 100 meters
    float3 world_pos = input.world_position.xyz * scaling_unit + float3(0.0, 0.0, atmos->planet_radius);
    float h = length(world_pos);

    float cos_theta = dot(world->sun.direction, world_pos / h);
    float2 transmittance_lut_uv = get_transmittance_lut_uv(atmos, float2(h, cos_theta));
    float3 transmittance_color = transmittance_image.sample(world->linear_sampler, transmittance_lut_uv).rgb;

    float3 radiance = max(0.0, dot(input.normal, world->sun.direction));
    return float4(transmittance_color, 1.0);
}
