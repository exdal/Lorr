import atmos;
import "../lorr";

[[vk::push_constant]] struct {
    World *world;
    ImageViewID sky_lut_image_id;
    SamplerID sampler_id;
    ImageViewID transmittance_lut_image_id;
    u32 pad;
} c;

struct VertexOutput {
    float4 position     : SV_Position;
    float4 world_position: POSITION;
    float2 tex_coord    : TEXCOORD;
};

[shader("vertex")]
VertexOutput vs_main(u32 vertex_id : SV_VertexID) {
    const let active_camera = c.world.cameras[c.world.active_camera_index];

    VertexOutput output;
    output.tex_coord = float2((vertex_id << 1) & 2, vertex_id & 2);
    output.position = float4(output.tex_coord * float2(2.0, 2.0) - float2(1.0, 1.0), 0.5, 1);
    float4 clip_space = float4(2.0 * output.tex_coord - 1.0, 1.0, 1.0);
    output.world_position = mul(active_camera.inv_projection_view_mat, clip_space);

    return output;
}

float draw_sun(float cos_theta, float sun_radius) {
    float deg = cos(sun_radius * PI / 180.0);
    if (cos_theta > deg) {
        return 1.0;
    }

    return 0.0;
}

vec3 aces(vec3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

vec3 jodieReinhard(vec3 c){
    vec3 tc = c / (c + 1.0);
    return lerp(c / (dot(c, vec3(0.2126, 0.7152, 0.0722)) + 1.0), tc, tc);
}

[shader("fragment")]
float4 fs_main(VertexOutput input) : SV_TARGET {
    const let active_camera = c.world.cameras[c.world.active_camera_index];
    Atmosphere atmos = c.world.atmosphere;
    const let sun = c.world.sun;
    const let sky_lut = ImageView(c.sky_lut_image_id);
    
    float3 world_pos = input.world_position.xyz / input.world_position.w;
    float3 direction = normalize(world_pos);

    float l = asin(direction.y);
    float u = atan2(direction.z, direction.x) / (2.0 * PI);
    float v = 0.5 - 0.5 * sign(l) * sqrt(abs(l) / (0.5 * PI));

    float3 color = sky_lut.sample(c.sampler_id, float2(u, v)).rgb;

    const let cos_theta = dot(sun.direction, vec3(0.0, 1.0, 0.0));
    vec3 transmittance = sample_lut(&atmos, c.transmittance_lut_image_id, c.sampler_id, active_camera.position.y, cos_theta);
    color += draw_sun(dot(sun.direction, direction), 1.0) * sun.intensity * transmittance;
    color = aces(color);

    return float4(color, 1.0);
}


