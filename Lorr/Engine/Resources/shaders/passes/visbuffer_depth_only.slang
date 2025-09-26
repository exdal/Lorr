import std;
import gpu;
import scene;
import visbuffer;

[[vk::binding(0)]] ConstantBuffer<Camera> camera;
[[vk::binding(1)]] StructuredBuffer<Mesh> meshes;
[[vk::binding(2)]] StructuredBuffer<MeshInstance> mesh_instances;
[[vk::binding(3)]] StructuredBuffer<MeshletInstance> meshlet_instances;
[[vk::binding(4)]] StructuredBuffer<Transform> transforms;

struct VertexOutput {
    f32x4 position : SV_Position;
};

[[shader("vertex")]]
func vs_main(u32 vertex_index : SV_VertexID) -> VertexOutput {
    let vis = VisBufferData(vertex_index);
    let meshlet_instance = meshlet_instances[vis.meshlet_instance_index];
    let mesh_instance = mesh_instances[meshlet_instance.mesh_instance_index];
    let mesh = meshes[mesh_instance.mesh_index];
    let mesh_lod = mesh.lods[mesh_instance.lod_index];
    let transform = transforms[mesh_instance.transform_index];
    let meshlet = mesh_lod.meshlets[meshlet_instance.meshlet_index];

    let index = meshlet.index(mesh_lod, vis.triangle_index);
    let vertex_pos = meshlet.position(mesh, index);
    let tex_coord = meshlet.tex_coord(mesh, index);
    let world_pos = transform.to_world_position(vertex_pos);
    let clip_pos = mul(camera.projection_view_mat, f32x4(world_pos.xyz, 1.0));

    VertexOutput output;
    output.position = clip_pos;

    return output;
}

[[shader("fragment")]]
func fs_main(VertexOutput input) -> f32x4 {
    return f32x4(1.0);
}
