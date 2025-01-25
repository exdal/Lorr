import atmos;
import "../lorr";

[[vk::binding(0, 0)]] SamplerState sampler;
[[vk::binding(1, 0)]] Texture2D<float4> transmittance_lut;
[[vk::binding(2, 0)]] Texture2D<float4> multiscattering_lut;
[[vk::binding(3, 0)]] Atmosphere atmos;
[[vk::binding(4, 0)]] Sun sun;
[[vk::binding(5, 0)]] Camera camera;

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
vec4 fs_main(VertexOutput input) : SV_TARGET {
    float3 eye_pos = float3(0.0, camera.position.y, 0.0) *
                     CAMERA_SCALE_UNIT +
                     float3(0.0, atmos.planet_radius, 0.0);

    f32 h = length(eye_pos);
    f32 u = fromSubUvsToUnit(input.tex_coord.x, 200);
    f32 v = fromSubUvsToUnit(input.tex_coord.y, 100);

    float horizon = sqrt(h * h - atmos.planet_radius * atmos.planet_radius);
    float cos_beta = horizon / h;
    float beta = acos(cos_beta);
    float zenith_horizon_angle = PI - beta;

    float view_zenith_cos_angle = 0.0;
    float light_view_cos_angle = -((u * u) * 2.0 - 1.0);
    // Non-linear parameterization
    if (v < 0.5) {
        f32 coord = 2.0 * v;
        coord = 1.0 - coord;
        coord *= coord;
        coord = 1.0 - coord;
        view_zenith_cos_angle = cos(zenith_horizon_angle * coord);
    } else {
        f32 coord = v * 2.0 - 1.0;
        coord *= coord;
        view_zenith_cos_angle = cos(zenith_horizon_angle + beta * coord);
    }

    float3 up_vec = eye_pos / h;
    float sun_zenith_cos_angle = dot(normalize(sun.direction), up_vec);
    float3 sun_dir = normalize(float3(sqrt(1.0 - sun_zenith_cos_angle * sun_zenith_cos_angle), sun_zenith_cos_angle, 0.0));

    float view_zenith_sin_angle = sqrt(1.0 - view_zenith_cos_angle * view_zenith_cos_angle);
    float3 eye_dir = float3(
        view_zenith_sin_angle * light_view_cos_angle,
        view_zenith_cos_angle,
        view_zenith_sin_angle * sqrt(1.0 - light_view_cos_angle * light_view_cos_angle)
    );

    if (!move_to_top_atmosphere(eye_pos, eye_dir, atmos.atmos_radius)) {
        return 0.0;
    }

    AtmosphereIntegrateInfo info = {};
    info.eye_pos = eye_pos;
    info.eye_dir = eye_dir;
    info.sun_dir = sun_dir;
    info.sun_intensity = sun.intensity;
    info.lut_sampler = sampler;
    info.transmittance_image = transmittance_lut;
    info.multiscattering_image = multiscattering_lut;

    const int sample_count = 30;
    info.sampling.variable_sample_count = true;
    info.sampling.min_sample_count = sample_count;
    info.sampling.max_sample_count = sample_count;

    const let result = integrate_single_scattered_luminance(atmos, info);
    const float transmittance = dot(result.transmittance, float3(1.0f / 3.0f, 1.0f / 3.0f, 1.0f / 3.0f));
    return float4(result.luminance, transmittance);
}

