module visbuffer_decode;

import std;
import gpu;
import scene;
import visbuffer;

#include <fullscreen.slang>

[[vk::binding(0, 0)]]
Image2D<u32> visbuffer;

[[vk::binding(1, 0)]]
ConstantBuffer<Camera, ScalarDataLayout> camera;

[[vk::binding(2, 0)]]
StructuredBuffer<u32, ScalarDataLayout> visible_meshlet_instances_indices;

[[vk::binding(3, 0)]]
StructuredBuffer<MeshletInstance, ScalarDataLayout> meshlet_instances;

[[vk::binding(4, 0)]]
StructuredBuffer<Model, ScalarDataLayout> models;

[[vk::binding(5, 0)]]
StructuredBuffer<Transform, ScalarDataLayout> transforms;

[[vk::binding(6, 0)]]
StructuredBuffer<Material, ScalarDataLayout> materials;

struct FragmentOutput {
    f32x4 albedo_color : SV_Target0;
    f32x4 normal_color : SV_Target1;
    f32x3 emission_color : SV_Target2;
    f32x4 metallic_roughness_occlusion_color : SV_Target3;
};

struct PartialDeriv {
    f32x3 uv_ddx;
    f32x3 uv_ddy;
    f32x3 lambda;

    [[mutating]]
    __init(in f32x4x3 clip_pos, in f32x2 uv, in f32x2 resolution) {
        // Partial Derivatives
        const f32x3 inv_w  = 1.0 / f32x3(clip_pos[0].w, clip_pos[1].w, clip_pos[2].w);
        const f32x2 ndc_0  = clip_pos[0].xy * inv_w[0];
        const f32x2 ndc_1  = clip_pos[1].xy * inv_w[1];
        const f32x2 ndc_2  = clip_pos[2].xy * inv_w[2];
        // Inverse area of a triangle.
        // https://cg.ivd.kit.edu/publications/2015/dais/DAIS.pdf
        // Appendix A:
        // D = 
        const f32 inv_det  = 1.0 / determinant(mat2(ndc_2 - ndc_1, ndc_0 - ndc_1));
        // Lambda 1 =
        this.uv_ddx = f32x3(ndc_1.y - ndc_2.y, ndc_2.y - ndc_0.y, ndc_0.y - ndc_1.y) * inv_det * inv_w;
        // Lambda 2 =
        this.uv_ddy = f32x3(ndc_2.x - ndc_1.x, ndc_0.x - ndc_2.x, ndc_1.x - ndc_0.x) * inv_det * inv_w;
        f32 ddx_sum  = dot(this.uv_ddx, 1.0);
        f32 ddy_sum  = dot(this.uv_ddy, 1.0);

        const f32x2 delta    = uv - ndc_0;
        const f32 view_inv_w = inv_w.x + delta.x * ddx_sum + delta.y * ddy_sum;
        const f32 view_w     = 1.0 / view_inv_w;
        this.lambda = {
            view_w * (inv_w.x + delta.x * this.uv_ddx.x + delta.y * this.uv_ddy.x),
            view_w * (0.0     + delta.x * this.uv_ddx.y + delta.y * this.uv_ddy.y),
            view_w * (0.0     + delta.x * this.uv_ddx.z + delta.y * this.uv_ddy.z)
        };
        const f32x2 two_over_resolution = 2.0 / resolution;
        this.uv_ddx *=  two_over_resolution.x;
        this.uv_ddy *= -two_over_resolution.y;
        ddx_sum     *=  two_over_resolution.x;
        ddy_sum     *= -two_over_resolution.y;

        const f32 view_ddx = 1.0 / (view_inv_w + ddx_sum);
        const f32 view_ddy = 1.0 / (view_inv_w + ddy_sum);
        this.uv_ddx = view_ddx * (this.lambda * view_inv_w + this.uv_ddx) - this.lambda;
        this.uv_ddy = view_ddy * (this.lambda * view_inv_w + this.uv_ddy) - this.lambda;
    }

    func interpolate(f32x3 v) -> f32 {
        return dot(this.lambda, v);
    }

    func interpolate_3x3(f32x3x3 v) -> f32x3 {
        return f32x3(
            dot(this.lambda, f32x3(v[0].x, v[1].x, v[2].x)),
            dot(this.lambda, f32x3(v[0].y, v[1].y, v[2].y)),
            dot(this.lambda, f32x3(v[0].z, v[1].z, v[2].z))
        );
    }

    func gradient_of(f32x2x3 v) -> UVGradient {
        const let v0 = f32x3(v[0].x, v[1].x, v[2].x);
        const let v1 = f32x3(v[0].y, v[1].y, v[2].y);

        UVGradient grad;
        grad.uv.x = this.interpolate(v0);
        grad.uv.y = this.interpolate(v1);
        grad.ddx.x = dot(this.uv_ddx, v0);
        grad.ddx.y = dot(this.uv_ddx, v1);
        grad.ddy.x = dot(this.uv_ddy, v0);
        grad.ddy.y = dot(this.uv_ddy, v1);

        return grad;
    }
};

[[shader("fragment")]]
func fs_main(VertexOutput input) -> FragmentOutput {
    const u32 texel = visbuffer.load(u32x2(input.position.xy));
    if (texel == ~0u) {
        discard;
    }

    FragmentOutput output = {};

    const VisBuffer vis = { texel };
    const u32 meshlet_instance_index = visible_meshlet_instances_indices[vis.meshlet_instance_index];
    const MeshletInstance meshlet_instance = meshlet_instances[meshlet_instance_index];
    const Model model = models[meshlet_instance.model_index];
    const Transform transform = transforms[meshlet_instance.transform_index];
    const Material material = materials[meshlet_instance.material_index];

    const Meshlet meshlet    = model.meshlets[meshlet_instance.meshlet_index];
    const Triangle indices   = meshlet.indices(model, vis.triangle_index);
    const u32x3 vertices     = meshlet.vertices(model, indices);
    const f32x3x3 positions  = meshlet.positions(model, vertices);
    const f32x3x3 normals    = meshlet.normals(model, vertices);
    const f32x2x3 tex_coords = meshlet.tex_coords(model, vertices);
    const f32x4x3 world_pos  = transform.to_world_positions(positions);
    const f32x4x3 clip_pos   = camera.to_clip_positions(world_pos);
    const PartialDeriv deriv = { clip_pos, input.tex_coord * 2.0 - 1.0, camera.resolution };

    const UVGradient tex_coord_grad = deriv.gradient_of(tex_coords);

    // ALBEDO ───────────────────────────────────────────────────────────
    output.albedo_color = material.sample_albedo_color(tex_coord_grad);

    // NORMALS ──────────────────────────────────────────────────────────
    const f32x3 normal_deriv = deriv.interpolate_3x3(normals);
    const f32x3 world_normal = mul(transform.normal, normal_deriv);
    f32x3 N = world_normal;
    if (material.normal_image_index != ~0u) {
        const f32x3x3 world_positions = f32x3x3(
            world_pos[0].xyz,
            world_pos[1].xyz,
            world_pos[2].xyz
        );

        const f32x3 pos_ddx = mul(deriv.uv_ddx, world_positions);
        const f32x3 pos_ddy = mul(deriv.uv_ddy, world_positions);

        const f32x3 pos_ddx_s = pos_ddx - dot(pos_ddx, N) * N;
        const f32x3 pos_ddy_s = pos_ddy - dot(pos_ddy, N) * N;
        const f32 jacobian_sign = sign(determinant(f32x2x2(tex_coord_grad.ddx, tex_coord_grad.ddy)));

        f32x3 T = jacobian_sign * (tex_coord_grad.ddy.y * pos_ddx_s - tex_coord_grad.ddx.y * pos_ddy);
        if (jacobian_sign != 0.0f) {
            T = normalize(T);
        }
        const f32x3 B = jacobian_sign * sign(dot(pos_ddy, cross(N, pos_ddx))) * cross(N, T);
        const f32x3x3 TBN = f32x3x3(T, -B, N);

        const f32x2 Ns = material.sample_normal_color(tex_coord_grad).xy * 2.0 - 1.0;
        const f32 Ns_z = sqrt(saturate(1.0f - dot(Ns, Ns)));
        N = mul(f32x3(Ns, Ns_z), TBN);
    }
    output.normal_color.xy = std::vec3_to_oct(N);
    output.normal_color.zw = std::vec3_to_oct(world_normal);

    // EMISSION ─────────────────────────────────────────────────────────
    output.emission_color = material.sample_emissive_color(tex_coord_grad);

    // METALLIC ROUGHNESS ───────────────────────────────────────────────
    const f32x2 metallic_roughness_color = material.sample_metallic_roughness(tex_coord_grad);
    output.metallic_roughness_occlusion_color.xy = metallic_roughness_color;

    // AMBIENT OCCLUSION ────────────────────────────────────────────────
    const f32 occlusion_color = material.sample_occlusion_color(tex_coord_grad);
    output.metallic_roughness_occlusion_color.z = occlusion_color;

    return output;
}
