import vis;
import "../lorr";

public typedef vector<uint8_t, 2> u8x2;
public typedef vector<uint8_t, 3> u8x3;
public typedef vector<uint8_t, 4> u8x4;

public typedef vector<uint16_t, 2> u16x2;
public typedef vector<uint16_t, 3> u16x3;
public typedef vector<uint16_t, 4> u16x4;

public typedef vector<uint32_t, 2> u32x2;
public typedef vector<uint32_t, 3> u32x3;
public typedef vector<uint32_t, 4> u32x4;

public typedef vector<uint64_t, 2> u64x2;
public typedef vector<uint64_t, 3> u64x3;
public typedef vector<uint64_t, 4> u64x4;

public typedef vector<int8_t, 2> i8x2;
public typedef vector<int8_t, 3> i8x3;
public typedef vector<int8_t, 4> i8x4;

public typedef vector<int16_t, 2> i16x2;
public typedef vector<int16_t, 3> i16x3;
public typedef vector<int16_t, 4> i16x4;

public typedef vector<int32_t, 2> i32x2;
public typedef vector<int32_t, 3> i32x3;
public typedef vector<int32_t, 4> i32x4;

public typedef vector<int64_t, 2> i64x2;
public typedef vector<int64_t, 3> i64x3;
public typedef vector<int64_t, 4> i64x4;

public typedef vector<float16_t, 2> f16x2;
public typedef vector<float16_t, 3> f16x3;
public typedef vector<float16_t, 4> f16x4;

public typedef vector<float32_t, 2> f32x2;
public typedef vector<float32_t, 3> f32x3;
public typedef vector<float32_t, 4> f32x4;

public typedef matrix<f32, 2, 2> f32x2x2;
public typedef matrix<f32, 3, 2> f32x2x3;
public typedef matrix<f32, 2, 3> f32x3x2;
public typedef matrix<f32, 3, 3> f32x3x3;
public typedef matrix<f32, 4, 3> f32x3x4;
public typedef matrix<f32, 3, 4> f32x4x3;
public typedef matrix<f32, 4, 4> f32x4x4;

public typedef vector<float64_t, 2> f64x2;
public typedef vector<float64_t, 3> f64x3;
public typedef vector<float64_t, 4> f64x4;

public typedef matrix<float, 4, 4> mat4;
public typedef matrix<float, 3, 3> mat3;
public typedef matrix<float, 2, 2> mat2;

[[vk::binding(0, 0)]] Texture2D<u32> visbuffer;

[[vk::binding(1, 0)]] ConstantBuffer<Scene, ScalarDataLayout> scene;

[[vk::binding(2, 0)]] restrict readonly
RWStructuredBuffer<Transform, ScalarDataLayout> transforms;

[[vk::binding(3, 0)]] restrict readonly
RWStructuredBuffer<Model, ScalarDataLayout> models;

[[vk::binding(4, 0)]] restrict readonly
RWStructuredBuffer<MeshletInstance, ScalarDataLayout> meshlet_instances;

[[vk::binding(5, 0)]] restrict readonly
RWStructuredBuffer<u32, ScalarDataLayout> visible_meshlet_instances_indices;

[[vk::binding(6, 0)]] restrict readonly
RWStructuredBuffer<Material, ScalarDataLayout> materials;

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

struct PartialDeriv {
    f32x3 uv_ddx;
    f32x3 uv_ddy;
    f32x3 lambda;

    [[mutating]]
    __init(in f32x4x3 clip_pos, in f32x2 uv) {
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
        // Convert UV[0, 1] to UV[-1, 1]
        const f32x2 delta    = ((uv - 0.5) * 2.0) - ndc_0;
        const f32 view_inv_w = inv_w.x + delta.x * ddx_sum + delta.y * ddy_sum;
        const f32 view_w     = 1.0 / view_inv_w;
        this.lambda = {
            view_w * (inv_w.x + delta.x * this.uv_ddx.x + delta.y * this.uv_ddy.x),
            view_w * (0.0     + delta.x * this.uv_ddx.y + delta.y * this.uv_ddy.y),
            view_w * (0.0     + delta.x * this.uv_ddx.z + delta.y * this.uv_ddy.z)
        };
        const f32x2 two_over_resolution = 2.0 / scene.camera.resolution;
        this.uv_ddx *=  two_over_resolution.x;
        this.uv_ddy *= -two_over_resolution.y;
        ddx_sum     *=  two_over_resolution.x;
        ddy_sum     *= -two_over_resolution.y;

        const f32 view_ddx = 1.0 / (view_inv_w + ddx_sum);
        const f32 view_ddy = 1.0 / (view_inv_w + ddy_sum);
        this.uv_ddx = view_ddx * (this.lambda * view_inv_w + this.uv_ddx) - this.lambda;
        this.uv_ddy = view_ddy * (this.lambda * view_inv_w + this.uv_ddy) - this.lambda;
    }

    func interpolate<let N : int>(vector<f32, N> x, vector<f32, N> y, vector<f32, N> z) -> vector<f32, N> {
        return x * vector<f32, N>(this.lambda.x) + y * vector<f32, N>(this.lambda.y) + z * vector<f32, N>(this.lambda.z);
    }

    func interpolate<let C : int, let R : int = 3>(matrix<f32, R, C> x) -> vector<f32, C> {
        return this.interpolate<C>(x[0], x[1], x[2]);
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

func sample_texture(u32 i, f32x2 gradient, f32x2 uv_ddx, f32x2 uv_ddy) -> float4 {
    return textures[i].SampleGrad(samplers[i], gradient, uv_ddx, uv_ddy);
}

struct FragmentOutput {
    float4 albedo : SV_Target0;
    float4 normal : SV_Target1;
    float3 emission : SV_Target2;
    float4 metallic_roughness_occlusion : SV_Target3;
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
    const u32 meshlet_instance_index = visible_meshlet_instances_indices[visible_meshlet_index];
    const MeshletInstance meshlet_instance = meshlet_instances[meshlet_instance_index];
    const Model model = models[meshlet_instance.mesh_index];
    const Transform transform = transforms[meshlet_instance.transform_index];

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
    const f32x3x3 positions = {
        model.vertex_positions[vertex_offset + indices.x],
        model.vertex_positions[vertex_offset + indices.y],
        model.vertex_positions[vertex_offset + indices.z],
    };
    const f32x3x3 normals = {
        mul(transform.normal, model.vertex_normals[vertex_offset + indices.x]),
        mul(transform.normal, model.vertex_normals[vertex_offset + indices.y]),
        mul(transform.normal, model.vertex_normals[vertex_offset + indices.z]),
    };
    const f32x2x3 tex_coords = {
        model.texture_coords[vertex_offset + indices.x],
        model.texture_coords[vertex_offset + indices.y],
        model.texture_coords[vertex_offset + indices.z],
    };
    const f32x4x3 world_positions = {
        mul(transform.world, float4(positions[0], 1.0)),
        mul(transform.world, float4(positions[1], 1.0)),
        mul(transform.world, float4(positions[2], 1.0)),
    };
    const f32x4x3 clip_positions = {
        mul(scene.camera.projection_view_mat, world_positions[0]),
        mul(scene.camera.projection_view_mat, world_positions[1]),
        mul(scene.camera.projection_view_mat, world_positions[2]),
    };
    const PartialDeriv deriv = { clip_positions, input.tex_coord };
    const Material material = materials[meshlet_instance.material_index];
    const f32x2 tex_coord_deriv = deriv.interpolate(tex_coords);
    const f32x2 tex_coord_ddx   = deriv.ddx_of(tex_coords);
    const f32x2 tex_coord_ddy   = deriv.ddy_of(tex_coords);

    // ALBEDO ───────────────────────────────────────────────────────────
    if (material.albedo_image_index != ~0u) {
        const float4 albedo_texture = sample_texture(material.albedo_image_index,
            tex_coord_deriv, tex_coord_ddx, tex_coord_ddy);
        output.albedo = material.albedo_color * albedo_texture;
    } else {
        output.albedo = material.albedo_color;
    }

    // NORMALS ──────────────────────────────────────────────────────────
    const f32x3 normal_deriv = deriv.interpolate(normals[0], normals[1], normals[2]);
    f32x3 N = normal_deriv;
    if (material.normal_image_index != ~0u) {
        // https://www.jeremyong.com/graphics/2023/12/16/surface-gradient-bump-mapping
        const f32x3x3 world_pos_3d = {
            world_positions[0].xyz,
            world_positions[1].xyz,
            world_positions[2].xyz
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
        const f32x3x3 TBN = transpose(f32x3x3(T, -B, N));
        const f32x2 sampled_normal = sample_texture(material.normal_image_index,
            tex_coord_deriv, tex_coord_ddx, tex_coord_ddy).xy * 2.0 - 1.0;
        const f32 sampled_normal_z = sqrt(max(1.0 - sampled_normal.x * sampled_normal.x - sampled_normal.y * sampled_normal.y, 0.0));
        N = mul(TBN, f32x3(sampled_normal, sampled_normal_z));
    }
    output.normal.xy = lr::vec3_to_oct(N);
    output.normal.zw = lr::vec3_to_oct(normal_deriv);

    // EMISSION ─────────────────────────────────────────────────────────
    if (material.emissive_image_index != ~0u) {
        const float3 emissive_texture = sample_texture(material.emissive_image_index,
            tex_coord_deriv, tex_coord_ddx, tex_coord_ddy).xyz;
        output.emission = material.emissive_color * emissive_texture;
    } else {
        output.emission = material.emissive_color;
    }

    // METALLIC ROUGHNESS ───────────────────────────────────────────────
    if (material.metallic_rougness_image_index != ~0u) {
        const float2 metallic_roughness_texture = sample_texture(material.metallic_rougness_image_index,
            tex_coord_deriv, tex_coord_ddx, tex_coord_ddy).bg;
        output.metallic_roughness_occlusion.xy = float2(material.metallic_factor, material.roughness_factor) * metallic_roughness_texture;
    } else {
        output.metallic_roughness_occlusion.xy = float2(material.metallic_factor, material.roughness_factor);
    }

    // AMBIENT OCCLUSION ────────────────────────────────────────────────
    if (material.occlusion_image_index != ~0u) {
        const float occlusion_texture = sample_texture(material.occlusion_image_index,
            tex_coord_deriv, tex_coord_ddx, tex_coord_ddy).r;
        output.metallic_roughness_occlusion.z = occlusion_texture;
    } else {
        output.metallic_roughness_occlusion.z = 1.0;
    }

    return output;
}
