module visbuffer_decode;

import std;
import gpu;
import scene;
import visbuffer;

struct VertexOutput {
    f32x4 position : SV_Position;
    f32x2 tex_coord : TEXCOORD;
};

[[shader("vertex")]]
func vs_main(u32 vertex_index : SV_VertexID) -> VertexOutput{
    VertexOutput output;
    output.tex_coord = f32x2((vertex_index << 1) & 2, vertex_index & 2);
    output.position = f32x4(2.0 * output.tex_coord - 1.0, 1.0, 1.0);

    return output;
}

[[vk::binding(0, 0)]]
Image2D<u32> visbuffer;

struct PushConstants {
    Camera *camera;
    u32 *visible_meshlet_instances_indices;
    MeshletInstance *meshlet_instances;
    Model *models;
    Transform *transforms;
    Material *materials;
};
[[vk::push_constant]] PushConstants C;

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

    func interpolate<T : IFloat>(T x, T y, T z) -> T {
        return x * T(this.lambda.x) + y * T(this.lambda.y) + z * T(this.lambda.z);
    }

    func ddx_of<let N : int>(vector<f32, N> x, vector<f32, N> y, vector<f32, N> z) -> vector<f32, N> {
        return x * vector<f32, N>(this.uv_ddx.x) + y * vector<f32, N>(this.uv_ddx.y) + z * vector<f32, N>(this.uv_ddx.z);
    }

    func ddx_of<let C : int, let R : int = 3>(matrix<f32, R, C> x) -> vector<f32, C> {
        return this.ddx_of(x[0], x[1], x[2]);
    }

    func ddy_of<let N : int>(vector<f32, N> x, vector<f32, N> y, vector<f32, N> z) -> vector<f32, N> {
        return x * vector<f32, N>(this.uv_ddy.x) + y * vector<f32, N>(this.uv_ddy.y) + z * vector<f32, N>(this.uv_ddy.z);
    }

    func ddy_of<let C : int, let R : int = 3>(matrix<f32, R, C> x) -> vector<f32, C> {
        return this.ddy_of(x[0], x[1], x[2]);
    }
};

[[shader("fragment")]]
func fs_main(VertexOutput input) -> FragmentOutput {
    const u32 texel = visbuffer.load(u32x2(input.position.xy));
    if (texel == ~0u) {
        discard;
    }

    const VisBuffer vis = { texel };
    const u32 meshlet_instance_index = C.visible_meshlet_instances_indices[vis.meshlet_instance_index];
    const MeshletInstance meshlet_instance = C.meshlet_instances[meshlet_instance_index];
    const Model model = C.models[meshlet_instance.model_index];
    const Transform transform = C.transforms[meshlet_instance.transform_index];
    const Material material = C.materials[meshlet_instance.material_index];

    const Meshlet meshlet    = model.meshlets[meshlet_instance.meshlet_index];
    const Triangle indices   = meshlet.indices(model, vis.triangle_index);
    const u32x3 vertices     = meshlet.vertices(model, indices);
    const f32x3x3 positions  = meshlet.positions(model, vertices);
    const f32x3x3 normals    = meshlet.normals(model, vertices);
    const f32x2x3 tex_coords = meshlet.tex_coords(model, vertices);
    const f32x4x3 world_pos  = transform.to_world_positions(positions);
    const f32x4x3 clip_pos   = C.camera.to_clip_positions(world_pos);
    const PartialDeriv deriv = { clip_pos, 2.0 * input.tex_coord - 1.0, C.camera.resolution };

    const f32x2 tex_coord_deriv = deriv.interpolate(tex_coords[0], tex_coords[1], tex_coords[2]);
    const f32x2 tex_coord_ddx   = deriv.ddx_of(tex_coords);
    const f32x2 tex_coord_ddy   = deriv.ddy_of(tex_coords);

    FragmentOutput output = {};

    // ALBEDO ───────────────────────────────────────────────────────────
    output.albedo_color = material.sample_albedo_color(
        tex_coord_deriv, tex_coord_ddx, tex_coord_ddy);

    // NORMALS ──────────────────────────────────────────────────────────
    const f32x3 normal_deriv = deriv.interpolate(normals[0], normals[1], normals[2]);
    const f32x3 world_normal = mul(transform.normal, normal_deriv);
    f32x3 N = world_normal;
    if (material.normal_image_index != ~0u) {
        // https://www.jeremyong.com/graphics/2023/12/16/surface-gradient-bump-mapping
        const f32x3x3 world_pos_3d = {
            world_pos[0].xyz,
            world_pos[1].xyz,
            world_pos[2].xyz
        };
        const f32x3 pos_ddx = deriv.ddx_of(world_pos_3d);
        const f32x3 pos_ddy = deriv.ddy_of(world_pos_3d);
        const f32x3 pos_ddx_s = pos_ddx - dot(pos_ddx, N) * N;
        const f32x3 pos_ddy_s = pos_ddy - dot(pos_ddy, N) * N;
        const f32 jacobian_sign = sign(determinant(mat2(tex_coord_ddx, tex_coord_ddy)));
        f32x3 T = jacobian_sign * (tex_coord_ddy.y * pos_ddx_s - tex_coord_ddx.y * pos_ddy_s);
        if (jacobian_sign != 0.0) {
            T = normalize(T);
        }
        const f32x3 B = jacobian_sign * sign(dot(pos_ddy, cross(N, pos_ddx))) * cross(N, T);
        const f32x3x3 TBN = f32x3x3(T, -B, N);
        const f32x2 sampled_normal = material.sample_normal_color(
            tex_coord_deriv, tex_coord_ddx, tex_coord_ddy).xy * 2.0 - 1.0;
        const f32 sampled_normal_z = sqrt(max(1.0 - sampled_normal.x * sampled_normal.x - sampled_normal.y * sampled_normal.y, 0.0));
        N = mul(f32x3(sampled_normal, sampled_normal_z), TBN);
    }
    output.normal_color.xy = std::vec3_to_oct(N);
    output.normal_color.zw = std::vec3_to_oct(world_normal);

    // EMISSION ─────────────────────────────────────────────────────────
    output.emission_color = material.sample_emissive_color(
        tex_coord_deriv, tex_coord_ddx, tex_coord_ddy);

    // METALLIC ROUGHNESS ───────────────────────────────────────────────
    const f32x2 metallic_roughness_color = material.sample_metallic_roughness(
        tex_coord_deriv, tex_coord_ddx, tex_coord_ddy);
    output.metallic_roughness_occlusion_color.xy = metallic_roughness_color;

    // AMBIENT OCCLUSION ────────────────────────────────────────────────
    const f32 occlusion_color = material.sample_occlusion_color(
        tex_coord_deriv, tex_coord_ddx, tex_coord_ddy);
    output.metallic_roughness_occlusion_color.z = occlusion_color;

    return output;
}
