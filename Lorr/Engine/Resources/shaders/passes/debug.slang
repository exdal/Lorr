module debug;

import gpu;
import scene;
import passes.visbuffer;

#include <fullscreen.slang>

[[vk::binding(0, 0)]] Sampler linear_repeat_sampler;
[[vk::binding(1, 0)]] Image2D<u32> visbuffer_data;
[[vk::binding(2, 0)]] Image2D<f32> depth_image;
[[vk::binding(3, 0)]] Image2D<u32> overdraw;
[[vk::binding(4, 0)]] Image2D<f32x4> albedo_image;
[[vk::binding(5, 0)]] Image2D<f32x4> normal_image;
[[vk::binding(6, 0)]] Image2D<f32x3> emissive_image;
[[vk::binding(7, 0)]] Image2D<f32x3> metallic_roughness_occlusion_image;

struct PushConstants {
    DebugView debug_view;
    f32 heatmap_scale;
    u32 *visible_meshlet_instances_indices;
    MeshletInstance *meshlet_instances;
    Transform *transforms;
    Mesh *meshes;
    Camera *camera;
};
[[vk::push_constant]] PushConstants C;

func hash(uint a) -> u32 {
    a = (a + 0x7ed55d16) + (a << 12);
    a = (a ^ 0xc761c23c) ^ (a >> 19);
    a = (a + 0x165667b1) + (a << 5);
    a = (a + 0xd3a2646c) ^ (a << 9);
    a = (a + 0xfd7046c5) + (a << 3);
    a = (a ^ 0xb55a4f09) ^ (a >> 16);
    return a;
}

func inferno(f32 t) -> f32x3 {
    let c0 = f32x3(0.0002189403691192265, 0.001651004631001012, -0.01948089843709184);
    let c1 = f32x3(0.1065134194856116, 0.5639564367884091, 3.932712388889277);
    let c2 = f32x3(11.60249308247187, -3.972853965665698, -15.9423941062914);
    let c3 = f32x3(-41.70399613139459, 17.43639888205313, 44.35414519872813);
    let c4 = f32x3(77.162935699427, -33.40235894210092, -81.80730925738993);
    let c5 = f32x3(-71.31942824499214, 32.62606426397723, 73.20951985803202);
    let c6 = f32x3(25.13112622477341, -12.24266895238567, -23.07032500287172);

    t = saturate(t);
    return c0 + t * (c1 + t * (c2 + t * (c3 + t * (c4 + t * (c5 + t * c6)))));
}

vector<T, N> min8<T : __BuiltinFloatingPointType, let N : int>(
    vector<T, N> p0, vector<T, N> p1, vector<T, N> p2,
    vector<T, N> p3, vector<T, N> p4, vector<T, N> p5,
    vector<T, N> p6, vector<T, N> p7) {
    return min(p0, min(p1, min(p2, min(p3, min(p4, min(p5, min(p6, p7)))))));
}

vector<T, N> max8<T : __BuiltinFloatingPointType, let N : int>(
    vector<T, N> p0, vector<T, N> p1, vector<T, N> p2,
    vector<T, N> p3, vector<T, N> p4, vector<T, N> p5,
    vector<T, N> p6, vector<T, N> p7) {
    return max(p0, max(p1, max(p2, max(p3, max(p4, max(p5, max(p6, p7)))))));
}

func project_aabb(f32x4x4 mvp, f32 near, f32x3 aabb_center, f32x3 aabb_extent) -> Optional<AABB> {
    let SX = mul(mvp, f32x4(aabb_extent.x, 0.0, 0.0, 0.0));
    let SY = mul(mvp, f32x4(0.0, aabb_extent.y, 0.0, 0.0));
    let SZ = mul(mvp, f32x4(0.0, 0.0, aabb_extent.z, 0.0));

    let aabb_half_extent = aabb_extent * 0.5;
    let P0 = mul(mvp, f32x4(aabb_center - aabb_half_extent, 1.0));
    let P1 = P0 + SZ;
    let P2 = P0 + SY;
    let P3 = P2 + SZ;
    let P4 = P0 + SX;
    let P5 = P4 + SZ;
    let P6 = P4 + SY;
    let P7 = P6 + SZ;

    let depth = min8(P0, P1, P2, P3, P4, P5, P6, P7).w;
    if (depth < near)
        return none;

    let DP0 = P0.xyz / P0.w;
    let DP1 = P1.xyz / P1.w;
    let DP2 = P2.xyz / P2.w;
    let DP3 = P3.xyz / P3.w;
    let DP4 = P4.xyz / P4.w;
    let DP5 = P5.xyz / P5.w;
    let DP6 = P6.xyz / P6.w;
    let DP7 = P7.xyz / P7.w;
    let min = min8(DP0, DP1, DP2, DP3, DP4, DP5, DP6, DP7);
    let max = max8(DP0, DP1, DP2, DP3, DP4, DP5, DP6, DP7);
    var vaabb = f32x4(min.xy, max.xy);
    vaabb = vaabb.xwzy * f32x4(0.5f, -0.5f, 0.5f, -0.5f) + 0.5f;
    return AABB(f32x3(vaabb.xy, min.z), f32x3(vaabb.zw, max.z));
}

[[shader("fragment")]]
func fs_main(VertexOutput input) -> f32x4 {
    let texel = visbuffer_data.load(u32x2(input.position.xy));
    if (texel == ~0u) {
        discard;
    }

    var color = f32x3(0.0, 0.0, 0.0);
    let vis_data = VisBufferData(texel);
    switch (C.debug_view) {
        case DebugView::Triangles: {
            let h = hash(vis_data.triangle_index);
            color = f32x3(f32(h & 255), f32((h >> 8) & 255), f32((h >> 16) & 255)) / 255.0;
        } break;
        case DebugView::Meshlets: {
            let visible_meshlet_instance_index = C.visible_meshlet_instances_indices[vis_data.meshlet_instance_index];
            let meshlet_instance = C.meshlet_instances[visible_meshlet_instance_index];
            let mesh = C.meshes[meshlet_instance.mesh_index];
            let meshlet_index = meshlet_instance.meshlet_index;
            let meshlet = mesh.meshlets[meshlet_index];
            let transform = C.transforms[meshlet_instance.transform_index];
            let bounds = mesh.meshlet_bounds[meshlet_index];
            let aabb_min = bounds.min;
            let aabb_max = bounds.max;
            let aabb_extent = aabb_max - aabb_min;
            let aabb_center = (aabb_min + aabb_max) * 0.5;
            let mvp = mul(C.camera->frustum_projection_view_mat, transform.world);
            if (let ndc_aabb = project_aabb(mvp, C.camera->near_clip, aabb_center, aabb_extent)) {
                AABB aabb = ndc_aabb;
                color.x = aabb.max.z;
            }

            //let h = hash(C.visible_meshlet_instances_indices[vis_data.meshlet_instance_index]);
            //color = f32x3(f32(h & 255), f32((h >> 8) & 255), f32((h >> 16) & 255)) / 255.0;
        } break;
        case DebugView::Overdraw: {
            const f32 draw_scale = clamp(C.heatmap_scale, 0.0, 100.0) / 100.0;
            const f32 draw_count = f32(overdraw.load(u32x2(input.position.xy)));
            const f32 heat = 1.0 - exp2(-draw_count * draw_scale);
            color = inferno(heat);
        } break;
        case DebugView::Albedo: {
            color = albedo_image.sample_mip(linear_repeat_sampler, input.tex_coord, 0).rgb;
        } break;
        case DebugView::Normal: {
            const f32x4 sampled_normal = normal_image.load(u32x2(input.position.xy));
            f32x3 mapped_normal = std::oct_to_vec3(sampled_normal.xy);
            color = mapped_normal * 0.5 + 0.5;
        } break;
        case DebugView::Emissive: {
            color = emissive_image.load(u32x2(input.position.xy)).rgb;
        } break;
        case DebugView::Metallic: {
            color = metallic_roughness_occlusion_image.load(u32x2(input.position.xy)).rrr;
        } break;
        case DebugView::Roughness: {
            color = metallic_roughness_occlusion_image.load(u32x2(input.position.xy)).ggg;
        } break;
        case DebugView::Occlusion: {
            color = metallic_roughness_occlusion_image.load(u32x2(input.position.xy)).bbb;
        } break;
        default:;
    }

    return f32x4(color, 1.0);
}
