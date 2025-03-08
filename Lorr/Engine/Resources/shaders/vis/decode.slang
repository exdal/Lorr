import vis;
import "../lorr";

[[vk::binding(0, 0)]] Texture2D<u32> visbuffer;

[[vk::binding(1, 0)]] ConstantBuffer<Scene, ScalarDataLayout> scene;

struct Transforms {
    Transform transforms[];
};
[[vk::binding(2, 0)]] restrict readonly
GLSLShaderStorageBuffer<Transforms, ScalarDataLayout> transforms;

struct Models {
    Model models[];
};
[[vk::binding(3, 0)]] restrict readonly
GLSLShaderStorageBuffer<Models, ScalarDataLayout> models;

struct MeshletInstances {
    MeshletInstance instances[];
};
[[vk::binding(4, 0)]] restrict readonly
GLSLShaderStorageBuffer<MeshletInstances, ScalarDataLayout> meshlet_instances;

struct VisibleMeshletInstancesIndices {
    u32 indices[];
};
[[vk::binding(5, 0)]] restrict readonly
GLSLShaderStorageBuffer<VisibleMeshletInstancesIndices, ScalarDataLayout> visible_meshlet_instances_indices;

struct Materials {
    Material materials[];
};
[[vk::binding(6, 0)]] restrict readonly
GLSLShaderStorageBuffer<Materials, ScalarDataLayout> materials;

[[vk::binding(0, 1)]] Texture2D<float4> textures[];
[[vk::binding(1, 1)]] SamplerState samplers[];

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

// - https://github.com/ConfettiFX/The-Forge/blob/02dcb2750aa857833e39c776b90479710c9c31fd/Common_3/Renderer/VisibilityBuffer/Shaders/FSL/VisibilityBufferShadingUtilities.h.fsl#L91
// - https://cg.ivd.kit.edu/publications/2015/dais/DAIS.pdf
struct PartialDeriv {
    float3 weight;
    float3 ddx;
    float3 ddy;
};
func calc_bary(in float3x4 clip, in float2 uv, in float2 resolution) -> PartialDeriv {
    PartialDeriv result;

    const let inv_w = 1.0 / float3(clip[0].w, clip[1].w, clip[2].w);
    const let ndc_0 = clip[0].xy * inv_w[0];
    const let ndc_1 = clip[1].xy * inv_w[1];
    const let ndc_2 = clip[2].xy * inv_w[2];

    // inverse of area of triangle
    // Appendix A:
    // D
    const let inv_det = 1.0 / determinant(float2x2(ndc_2 - ndc_1, ndc_0 - ndc_1));
    // Lambda 1
    result.ddx = float3(ndc_1.y - ndc_2.y, ndc_2.y - ndc_0.y, ndc_0.y - ndc_1.y) * inv_det * inv_w;
    // Lambda 2
    result.ddy = float3(ndc_2.x - ndc_1.x, ndc_0.x - ndc_2.x, ndc_1.x - ndc_0.x) * inv_det * inv_w;

    float ddx_sum = dot(result.ddx, float3(1.0));
    float ddy_sum = dot(result.ddy, float3(1.0));
    const let delta = uv - ndc_0;
    const let interp_inv_w = inv_w.x + delta.x * ddx_sum + delta.y * ddy_sum;
    const let interp_w = 1.0 / interp_inv_w;
    result.weight.x = interp_w * (inv_w.x + delta.x * result.ddx.x + delta.y * result.ddy.x);
    result.weight.y = interp_w * (0.0     + delta.x * result.ddx.y + delta.y * result.ddy.y);
    result.weight.z = interp_w * (0.0     + delta.x * result.ddx.z + delta.y * result.ddy.z);

    const let two_over_resolution = 2.0 / resolution;
    result.ddx *= two_over_resolution.x;
    result.ddy *= two_over_resolution.y;
    ddx_sum *= two_over_resolution.x;
    ddy_sum *= two_over_resolution.y;

    const let interp_ddx_w = 1.0 / (interp_inv_w + ddx_sum);
    const let interp_ddy_w = 1.0 / (interp_inv_w + ddy_sum);
    result.ddx = interp_ddx_w * (result.weight * interp_inv_w + result.ddx) - result.weight;
    result.ddy = interp_ddy_w * (result.weight * interp_inv_w + result.ddy) - result.weight;

    return result;
}

func interpolate(in PartialDeriv d, in float3 v) -> float3 {
    return float3(dot(v, d.weight), dot(v, d.ddx), dot(v, d.ddy));
}

func interpolate3(in PartialDeriv d, in float3x3 v) -> float3 {
    return float3(
        interpolate(d, float3(v[0].x, v[1].x, v[2].x)).x,
        interpolate(d, float3(v[0].y, v[1].y, v[2].y)).x,
        interpolate(d, float3(v[0].z, v[1].z, v[2].z)).x
    );
}

func interpolate4(in PartialDeriv d, in float3x4 v) -> float4 {
    return float4(
        interpolate(d, float3(v[0].x, v[1].x, v[2].x)).x,
        interpolate(d, float3(v[0].y, v[1].y, v[2].y)).x,
        interpolate(d, float3(v[0].z, v[1].z, v[2].z)).x,
        interpolate(d, float3(v[0].w, v[1].w, v[2].w)).x
    );
}

struct UVGradient {
    float2 gradient;
    float2 ddx;
    float2 ddy;
};
func calc_uv_gradient(in PartialDeriv d, in float3x2 tex_coords) -> UVGradient {
    const float2x3 interp_uv = float2x3(
        interpolate(d, float3(tex_coords[0].x, tex_coords[1].x, tex_coords[2].x)),
        interpolate(d, float3(tex_coords[0].y, tex_coords[1].y, tex_coords[2].y)),
    );
    UVGradient result;
    result.gradient = float2(interp_uv[0].x, interp_uv[1].x);
    result.ddx = float2(interp_uv[0].y, interp_uv[1].y);
    result.ddy = float2(interp_uv[0].z, interp_uv[1].z);

    return result;
}

func sample_texture(u32 i, UVGradient uv_gradient) -> float4 {
    return textures[i].SampleGrad(samplers[i], uv_gradient.gradient, uv_gradient.ddx, uv_gradient.ddy);
}

struct FragmentOutput {
    float4 albedo : SV_Target0;
    float4 normal : SV_Target1;
    float3 emission : SV_Target2;
};

[[vk::push_constant]] float2 resolution;

[shader("fragment")]
FragmentOutput fs_main(VertexOutput input, float4 pixel_pos : SV_Position) {
    FragmentOutput output;

    const ivec2 uv = ivec2(pixel_pos.xy);
    const u32 pixel = visbuffer.Load(ivec3(uv, 0));
    if (pixel == ~0u) {
        discard;
    }

    const u32 visible_meshlet_index = (pixel >> MESHLET_PRIMITIVE_BITS) & MESHLET_ID_MASK;
    const u32 triangle_index = pixel & MESHLET_PRIMITIVE_MASK;
    const u32 meshlet_instance_index = visible_meshlet_instances_indices.indices[visible_meshlet_index];
    const MeshletInstance meshlet_instance = meshlet_instances.instances[meshlet_instance_index];
    const Model model = models.models[meshlet_instance.model_index];
    const Transform transform = transforms.transforms[meshlet_instance.transform_index];

    const Meshlet meshlet = model.meshlets[meshlet_instance.meshlet_index];
    const u32 vertex_offset = meshlet.vertex_offset;
    const u32 index_offset = meshlet.index_offset;
    const u32 triangle_offset = meshlet.triangle_offset;
    const uvec3 triangle_indices = {
        u32(model.local_triangle_indices[triangle_offset + triangle_index * 3 + 0]),
        u32(model.local_triangle_indices[triangle_offset + triangle_index * 3 + 1]),
        u32(model.local_triangle_indices[triangle_offset + triangle_index * 3 + 2]),
    };
    const uvec3 indices = {
        model.indices[index_offset + triangle_indices.x],
        model.indices[index_offset + triangle_indices.y],
        model.indices[index_offset + triangle_indices.z],
    };
    const float3x3 positions = {
        model.vertex_positions[vertex_offset + indices.x],
        model.vertex_positions[vertex_offset + indices.y],
        model.vertex_positions[vertex_offset + indices.z],
    };
    const float3x2 tex_coords = {
        model.texture_coords[vertex_offset + indices.x],
        model.texture_coords[vertex_offset + indices.y],
        model.texture_coords[vertex_offset + indices.z],
    };
    const float3x4 world_positions = {
        mul(transform.world, float4(positions[0], 1.0)),
        mul(transform.world, float4(positions[1], 1.0)),
        mul(transform.world, float4(positions[2], 1.0)),
    };
    const float3x4 clip_positions = {
        mul(scene.camera.projection_view_mat, world_positions[0]),
        mul(scene.camera.projection_view_mat, world_positions[1]),
        mul(scene.camera.projection_view_mat, world_positions[2]),
    };
    const PartialDeriv partial_deriv = calc_bary(clip_positions, input.tex_coord * 2.0 - 1.0, resolution);
    const UVGradient uv_gradient = calc_uv_gradient(partial_deriv, tex_coords);
    const Material material = materials.materials[meshlet_instance.material_index];

    // ALBEDO ───────────────────────────────────────────────────────────
    if (material.albedo_image_index != ~0u) {
        output.albedo = sample_texture(material.albedo_image_index, uv_gradient);
    } else {
        output.albedo = material.albedo_color;
    }

    // NORMALS ──────────────────────────────────────────────────────────
    const float3 triangle_normal = normalize(cross(positions[1] - positions[0], positions[2] - positions[0]));
    const float3 world_normal = normalize(mul(transform.normal, triangle_normal));
    float3 final_normal = world_normal;

    if (material.normal_image_index != ~0u) {
        const float3x3 interp_view_positions = {
            interpolate(partial_deriv, float3(world_positions[0].x, world_positions[1].x, world_positions[2].x)),
            interpolate(partial_deriv, float3(world_positions[0].y, world_positions[1].y, world_positions[2].y)),
            interpolate(partial_deriv, float3(world_positions[0].z, world_positions[1].z, world_positions[2].z)),
        };
        const float3 ddx_pos = float3(interp_view_positions[0].y, interp_view_positions[1].y, interp_view_positions[2].y);
        const float3 ddy_pos = float3(interp_view_positions[0].z, interp_view_positions[1].z, interp_view_positions[2].z);
        const float2 ddx_tex_coord = uv_gradient.ddx;
        const float2 ddy_tex_coord = uv_gradient.ddy;

        const float3 N = final_normal;
        const float3 T = normalize(ddx_pos * ddy_tex_coord.y - ddy_pos * ddx_tex_coord.y);
        const float3 B = -normalize(cross(N, T));
        const mat3 TBN = { T, B, N };

        float3 sampled_normal = float3(sample_texture(material.normal_image_index, uv_gradient).xy, 1.0) * 2.0 - 1.0;
        final_normal = normalize(mul(TBN, sampled_normal));
    }

    output.normal.xy = lr::vec3_to_oct(final_normal);
    output.normal.zw = lr::vec3_to_oct(triangle_normal);

    // EMISSION ─────────────────────────────────────────────────────────
    if (material.emissive_image_index != ~0u) {
        const float3 emissive_texture = sample_texture(material.emissive_image_index, uv_gradient).xyz;
        output.emission = material.emissive_color * emissive_texture;
    } else {
        output.emission = material.emissive_color;
    }

    return output;
}
