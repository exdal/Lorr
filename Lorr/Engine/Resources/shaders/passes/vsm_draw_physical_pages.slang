import std;
import gpu;
import scene;
import visbuffer;
import vsm;

[[vk::binding(0)]] ConstantBuffer<DirectionalLight> directional_light;
[[vk::binding(1)]] StructuredBuffer<Mesh> meshes;
[[vk::binding(2)]] StructuredBuffer<MeshInstance> mesh_instances;
[[vk::binding(3)]] StructuredBuffer<MeshletInstance> meshlet_instances;
[[vk::binding(4)]] StructuredBuffer<Transform> transforms;
[[vk::binding(5)]] StructuredBuffer<VirtualClipmap> clipmaps;
[[vk::binding(6)]] Texture2DArray<u32> page_tables;
[[vk::binding(7)]] StorageImage2D<u32, ImageFormat::R32UI> physical_pages;

struct PushConstants {
    i32x3 page_table_extent;
    u32 clipmap_index;
    i32x3 physical_page_extent;
    u32 page_size;
};
[[vk::push_constant]] PushConstants C;

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

    let projection_view_mat = clipmaps[C.clipmap_index].projection_view_mat;
    let clip_pos = mul(projection_view_mat, f32x4(world_pos.xyz, 1.0));

    VertexOutput output;
    output.position = clip_pos;

    return output;
}

[[shader("fragment")]]
func fs_main(VertexOutput input) -> void {
    let physical_page_extent = f32x2(C.physical_page_extent.xy);
    let page_table_extent = f32x2(C.page_table_extent.xy);

    let clipmap = clipmaps[C.clipmap_index];
    let page_offset = clipmap.page_offset;

    let virtual_uv = input.position.xy / physical_page_extent;
    let wrapped_coords = vsm_clip_info_to_wrapped_coords(virtual_uv, page_offset, page_table_extent);
    let page_address = i32x3(i32x2(wrapped_coords), C.clipmap_index);
    let page = VSMPageMetadata(page_tables.Load(i32x4(page_address, 0)));
    if (page.is_backed() && page.is_dirty()) {
        let physical_page_address = page.get_physical_address();
        let physical_page_count = C.physical_page_extent.x / C.page_size;
        let physical_page_coords = i32x2(
            physical_page_address / physical_page_count,
            physical_page_address % physical_page_count
        );

        let physical_page_texel_coord = virtual_uv_to_physical_texel(virtual_uv, physical_page_coords, C.page_size, physical_page_extent);
        __atomic_max(physical_pages[physical_page_texel_coord], asuint(saturate(input.position.z / input.position.w)));
    }
}
