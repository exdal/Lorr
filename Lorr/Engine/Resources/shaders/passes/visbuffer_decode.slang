module visbuffer_decode;

import std;
import gpu;
import scene;
import passes.visbuffer;

#include <fullscreen.slang>

struct ShaderParameters {
    ConstantBuffer<Camera> camera;
    StructuredBuffer<MeshletInstance> meshlet_instances;
    StructuredBuffer<MeshInstance> mesh_instances;
    StructuredBuffer<Mesh> meshes;
    StructuredBuffer<Transform> transforms;
    StructuredBuffer<Material> materials;

    Image2D<u32> visbuffer;
};
ParameterBlock<ShaderParameters> params;

struct FragmentOutput {
    f32x4 albedo_color : SV_Target0;
    f32x4 normal_color : SV_Target1;
    f32x3 emission_color : SV_Target2;
    f32x4 metallic_roughness_occlusion_color : SV_Target3;
};

struct PartialDeriv {
    f32x3 ddx;
    f32x3 ddy;
    f32x3 lambda;
};

func compute_partial_derivatives(in f32x4x3 world_positions, in f32x2 uv, in f32x2 resolution) -> PartialDeriv {
    PartialDeriv result;

    let clip_pos_0 = mul(params.camera.projection_view_mat, f32x4(world_positions[0].xyz, 1.0));
    let clip_pos_1 = mul(params.camera.projection_view_mat, f32x4(world_positions[1].xyz, 1.0));
    let clip_pos_2 = mul(params.camera.projection_view_mat, f32x4(world_positions[2].xyz, 1.0));

    // Partial Derivatives
    let inv_w = 1.0 / f32x3(clip_pos_0.w, clip_pos_1.w, clip_pos_2.w);
    let ndc_0 = clip_pos_0.xy * inv_w[0];
    let ndc_1 = clip_pos_1.xy * inv_w[1];
    let ndc_2 = clip_pos_2.xy * inv_w[2];
    // Inverse area of a triangle.
    // https://cg.ivd.kit.edu/publications/2015/dais/DAIS.pdf
    // Appendix A:
    // D =
    let inv_det = 1.0 / determinant(f32x2x2(ndc_2 - ndc_1, ndc_0 - ndc_1));
    // Lambda 1 =
    result.ddx = f32x3(ndc_1.y - ndc_2.y, ndc_2.y - ndc_0.y, ndc_0.y - ndc_1.y) * inv_det * inv_w;
    // Lambda 2 =
    result.ddy = f32x3(ndc_2.x - ndc_1.x, ndc_0.x - ndc_2.x, ndc_1.x - ndc_0.x) * inv_det * inv_w;
    var ddx_sum = dot(result.ddx, 1.0);
    var ddy_sum = dot(result.ddy, 1.0);

    let delta_v = uv - ndc_0;
    let interp_inv_w = inv_w.x + delta_v.x * ddx_sum + delta_v.y * ddy_sum;
    let interp_w = 1.0 / interp_inv_w;
    result.lambda = f32x3(
        interp_w * (inv_w[0] + delta_v.x * result.ddx.x + delta_v.y * result.ddy.x),
        interp_w * (delta_v.x * result.ddx.y + delta_v.y * result.ddy.y),
        interp_w * (delta_v.x * result.ddx.z + delta_v.y * result.ddy.z)
    );
    let inv_resolution = 1.0 / resolution;
    result.ddx *= inv_resolution.x;
    result.ddy *= -inv_resolution.y;
    ddx_sum *= inv_resolution.x;
    ddy_sum *= -inv_resolution.y;

    let interp_ddx_w = 1.0 / (interp_inv_w + ddx_sum);
    let interp_ddy_w = 1.0 / (interp_inv_w + ddy_sum);
    result.ddx = interp_ddx_w * (result.lambda * interp_inv_w + result.ddx) - result.lambda;
    result.ddy = interp_ddy_w * (result.lambda * interp_inv_w + result.ddy) - result.lambda;

    return result;
}

func prepare_world_normal(
    world_normal: f32x3,
    double_sided: bool,
    is_front: bool,
) -> f32x3 {
    var output = world_normal;
    // NOTE: When NOT using normal-mapping, if looking at the back face of a double-sided
    // material, the normal needs to be inverted. This is a branchless version of that.
    output = (f32(!double_sided || is_front) * 2.0 - 1.0) * output;
    return normalize(output);
}

func calculate_world_tangent(
    world_normal: f32x3,
    ddx_world_position: f32x3,
    ddy_world_position: f32x3,
    ddx_uv: f32x2,
    ddy_uv: f32x2,
) -> f32x4 {
    let ddx_world_position_s = ddx_world_position - dot(ddx_world_position, world_normal) * world_normal;
    let ddy_world_position_s = ddy_world_position - dot(ddy_world_position, world_normal) * world_normal;
    let jacobian_sign = sign(ddx_uv.x * ddy_uv.y - ddx_uv.y * ddy_uv.x);
    var world_tangent = jacobian_sign * (ddy_uv.y * ddx_world_position_s - ddx_uv.y * ddy_world_position_s);
    if (jacobian_sign != 0.0) {
        world_tangent = normalize(world_tangent);
    }

    let w = jacobian_sign * sign(dot(ddy_world_position, cross(world_normal, ddx_world_position)));
    return f32x4(world_tangent, -w);
}

func calculate_tbn_mikktspace(world_normal: f32x3, world_tangent: f32x4) -> f32x3x3 {
    // NOTE: The mikktspace method of normal mapping explicitly requires that the world normal NOT
    // be re-normalized in the fragment shader. This is primarily to match the way mikktspace
    // bakes vertex tangents and normal maps so that this is the exact inverse. Blender, Unity,
    // Unreal Engine, Godot, and more all use the mikktspace method. Do not change this code
    // unless you really know what you are doing.
    // http://www.mikktspace.com/
    var N = world_normal;

    // NOTE: The mikktspace method of normal mapping explicitly requires that these NOT be
    // normalized nor any Gram-Schmidt applied to ensure the vertex normal is orthogonal to the
    // vertex tangent! Do not change this code unless you really know what you are doing.
    // http://www.mikktspace.com/
    var T = world_tangent.xyz;
    var B = world_tangent.w * cross(N, T);

#if 1
    // https://www.jeremyong.com/graphics/2023/12/16/surface-gradient-bump-mapping/#a-note-on-mikktspace-usage
    let inverse_length_n = 1.0 / length(N);
    T *= inverse_length_n;
    B *= inverse_length_n;
    N *= inverse_length_n;
#endif

    return f32x3x3(T, B, N);
}

[[shader("fragment")]]
func fs_main(
    VertexOutput input,
    bool is_front : SV_IsFrontFace
) -> FragmentOutput {
    let texel = params.visbuffer.Load(u32x3(u32x2(input.position.xy), 0));
    if (texel == ~0u) {
        discard;
    }

    FragmentOutput output = {};
    let vis = VisBufferData(texel);
    let meshlet_instance_index = vis.meshlet_instance_index;
    let meshlet_instance = params.meshlet_instances[meshlet_instance_index];
    let mesh_instance = params.mesh_instances[meshlet_instance.mesh_instance_index];
    let mesh = params.meshes[mesh_instance.mesh_index];
    let material = params.materials[mesh_instance.material_index];
    let transform = params.transforms[mesh_instance.transform_index];
    let mesh_lod = mesh.lods[mesh_instance.lod_index];
    let meshlet = mesh_lod.meshlets[meshlet_instance.meshlet_index];

    let indices = meshlet.indices(mesh_lod, vis.triangle_index);
    if (any(indices > (mesh.vertex_count - 1))) {
        // we are somehow OOB'ing
        return output;
    }

    let positions = meshlet.positions(mesh, indices);
    let normals = meshlet.normals(mesh, indices);
    let world_positions = transform.to_world_positions(positions);
    let NDC = f32x3(input.tex_coord * 2.0 - 1.0, 1.0);
    let deriv = compute_partial_derivatives(world_positions, NDC.xy, params.camera.resolution);

    let tex_coords = meshlet.tex_coords(mesh, indices);
    let uv = mul(deriv.lambda, tex_coords);
    let uv_ddx = mul(deriv.ddx, tex_coords);
    let uv_ddy = mul(deriv.ddy, tex_coords);

    // ALBEDO ───────────────────────────────────────────────────────────
    output.albedo_color = material.sample_albedo_color(uv, uv_ddx, uv_ddy);

    // NORMALS ──────────────────────────────────────────────────────────
    let camera_relative_world_positions = f32x3x3(
        world_positions[0].xyz - params.camera.position,
        world_positions[1].xyz - params.camera.position,
        world_positions[2].xyz - params.camera.position,
    );
    let pos_ddx = mul(deriv.ddx, camera_relative_world_positions);
    let pos_ddy = mul(deriv.ddy, camera_relative_world_positions);

    let double_sided = false;

    let world_normals = transform.to_world_normals(normals);
    var world_normal = mul(deriv.lambda, world_normals);
    var normal = prepare_world_normal(world_normal, double_sided, is_front);

    if ((material.flags & MaterialFlag::HasNormalImage) != 0) {
        let sampled_normal = material.sample_normal_color(uv, uv_ddx, uv_ddy);
        let world_tangent = calculate_world_tangent(world_normal, pos_ddx, pos_ddy, uv_ddx, uv_ddy);
        let TBN = calculate_tbn_mikktspace(normal, world_tangent);
        let T = TBN[0];
        let B = TBN[1];
        let N = TBN[2];
        var Nt = sampled_normal;
        if (material.flags & MaterialFlag::NormalTwoComponent) {
            Nt = f32x3(Nt.rg * 2.0 - 1.0, 0.0);
            Nt.z = sqrt(1.0 - Nt.x * Nt.x - Nt.y * Nt.y);
        } else {
            Nt = Nt * 2.0 - 1.0;
        }

        if (material.flags & MaterialFlag::NormalFlipY) {
            Nt.y = -Nt.y;
        }

        if (double_sided && !is_front) {
            Nt = -Nt;
        }

        normal = normalize(Nt.x * T + Nt.y * B + Nt.z * N);
    }

    output.normal_color.xy = std::octahedral_encode(normal);
    output.normal_color.zw = std::octahedral_encode(world_normal);

    // EMISSION ─────────────────────────────────────────────────────────
    output.emission_color = material.sample_emissive_color(uv, uv_ddx, uv_ddy);

    // METALLIC ROUGHNESS ───────────────────────────────────────────────
    let metallic_roughness_color = material.sample_metallic_roughness(uv, uv_ddx, uv_ddy);
    output.metallic_roughness_occlusion_color.xy = metallic_roughness_color;

    // AMBIENT OCCLUSION ────────────────────────────────────────────────
    let occlusion_color = material.sample_occlusion_color(uv, uv_ddx, uv_ddy);
    output.metallic_roughness_occlusion_color.z = occlusion_color;

    return output;
}