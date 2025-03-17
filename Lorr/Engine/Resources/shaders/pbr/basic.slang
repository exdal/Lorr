import "../lorr";
import "../atmos/atmos";

[[vk::binding(0, 0)]] ConstantBuffer<Scene> scene;

// We will be using sun transmittance as global environment map
[[vk::binding(1, 0)]] Texture2D<float4> sun_transmittance_lut;
[[vk::binding(2, 0)]] SamplerState linear_clamp_sampler;

// G-Buffer
[[vk::binding(3, 0)]] Texture2D<f32> depth_image;
[[vk::binding(4, 0)]] Texture2D<float4> albedo_image;
[[vk::binding(5, 0)]] Texture2D<float4> normal_image;
[[vk::binding(6, 0)]] Texture2D<float3> emissive_image;
[[vk::binding(7, 0)]] Texture2D<float4> metallic_roughness_image;

struct VertexOutput {
    float4 position : SV_Position;
    float2 tex_coord : TEXCOORD;
};

[shader("vertex")]
VertexOutput vs_main(u32 vertex_id : SV_VertexID) {
    VertexOutput output;
    output.tex_coord = float2((vertex_id << 1) & 2, vertex_id & 2);
    output.position = float4(2.0 * output.tex_coord - 1.0, 1.0, 1.0);

    return output;
}

float D_GGX(float NoH, float roughness2) {
    float f = ((roughness2 - 1.0) * NoH * NoH + 1.0);
    return roughness2 / (PI * f * f);
}

float Smith_G2_Height_Correlated_GGX_Lagarde(float NoV, float NoL, float roughness2) {
    float GGXL = NoV * sqrt((-NoL * roughness2 + NoL) * NoL + roughness2);
    float GGXV = NoL * sqrt((-NoV * roughness2 + NoV) * NoV + roughness2);
    return saturate(0.5f / (GGXV + GGXL));
}

float3 F_Schlick(float LoH, float3 specular_albedo) {
    return specular_albedo + (1.0 - specular_albedo) * pow(1.0 - LoH, 5.0);
}

float Fd_Lambert() {
    return 1.0 / PI;
}

float3 BRDF(float3 V, float3 N, float3 L, float3 albedo, float roughness, float metallic) {
    float3 H = normalize(V + L);
    float NoV = abs(dot(N, V)) + 1e-5;
    float NoL = max(dot(N, L), 0.0);
    float NoH = max(dot(N, H), 0.0);
    float LoH = max(dot(L, H), 0.0);

    const float reflectance = 0.5;
    float3 F0 = 0.16 * reflectance * reflectance * (1.0 - metallic) + albedo * metallic;

    float  roughness2 = roughness * roughness;
    float  D = D_GGX(NoH, roughness2);
    float3 V = Smith_G2_Height_Correlated_GGX_Lagarde(NoV, NoL, roughness2);
    float3 F = F_Schlick(LoH, F0);

    // Reflectance (Cook-Torrance)
    // V already divided by denominator
    float3 F_R = (D * V * NoL) * F;
    // Diffuse
    float3 F_D = albedo * Fd_Lambert();
    // Frensel combination
    float3 result = (1.0 - F) * (1.0 - metallic) * F_D + F_R;

    return result;
}

[shader("fragment")]
float4 fs_main(VertexOutput input, float4 frag_pos : SV_Position) : SV_Target {
    const int3 pixel_pos = int3(int2(frag_pos.xy), 0);

    float depth = depth_image.Load(pixel_pos);
    if (depth == 1.0) {
        discard;
    }

    const float3 albedo_color = albedo_image.Load(pixel_pos).rgb;
    const float4 normal_oct_flat = normal_image.Load(pixel_pos);
    const float3 mapped_normal = lr::oct_to_vec3(normal_oct_flat.xy);
    const float3 smooth_normal = lr::oct_to_vec3(normal_oct_flat.zw);
    const float3 emission = emissive_image.Load(pixel_pos);
    const float4 metallic_roughness = metallic_roughness_image.Load(pixel_pos);
    const float  metallic = metallic_roughness.x;
    const float  roughness = metallic_roughness.y;

    const float4 NDC = float4(input.tex_coord * 2.0 - 1.0, depth, 1.0);
    const float4 world_position = mul(scene.camera.inv_projection_view_mat, NDC);
    const float3 ray_pos = world_position.xyz / world_position.w;

    // PBR constants
    const float3 N = normalize(lerp(smooth_normal, mapped_normal, length(mapped_normal)));
    const float3 V = normalize(scene.camera.position - ray_pos);
    const float3 L = normalize(scene.sun.direction);  // temp

    const float3 ray_pos_planet = float3(0.0, ray_pos.y, 0.0) *
                            CAMERA_SCALE_UNIT +
                            float3(0.0, scene.atmosphere.planet_radius, 0.0);
    float h = length(ray_pos_planet);
    float3 up_vec = normalize(ray_pos_planet);
    float sun_cos_theta = dot(L, up_vec);
    float2 transmittance_uv = get_transmittance_lut_uv(scene.atmosphere, float2(h, sun_cos_theta));
    float3 sun_transmittance = sun_transmittance_lut.Sample(linear_clamp_sampler, transmittance_uv).rgb;

    const float NoL = max(dot(N, L), 0.0);
    float3 color = BRDF(V, N, L, albedo_color, roughness, metallic);
    color = color * NoL * sun_transmittance * scene.sun.intensity;

    color += (albedo_color * 0.03);

    return float4(color, 1.0);
}

