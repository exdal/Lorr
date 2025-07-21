module visbuffer_encode;

import std;
import gpu;
import scene;
import passes.visbuffer;

struct ShaderParameters {
    ConstantBuffer<Camera> camera;
    RWStructuredBuffer<u32> visible_meshlet_instances_indices;
    StructuredBuffer<MeshletInstance> meshlet_instances;
    StructuredBuffer<Mesh> meshes;
    StructuredBuffer<Transform> transforms;
    StructuredBuffer<Material, ScalarDataLayout> materials;
    StorageImage2D<u32, ImageFormat::R32UI> overdraw;
};
ParameterBlock<ShaderParameters> params;

struct VertexOutput {
    f32x4 position : SV_Position;
    f32x3 vertex_pos : POSITION;
    f32x2 tex_coord : TEXCOORD;
    nointerpolation u32 meshlet_instance_index;
    nointerpolation u32 triangle_index;
    nointerpolation u32 material_index;
};

[[shader("vertex")]]
func vs_main(u32 vertex_index : SV_VertexID) -> VertexOutput {
    let vis = VisBufferData(vertex_index);
    let meshlet_instance_index = params.visible_meshlet_instances_indices[vis.meshlet_instance_index];
    let meshlet_instance = params.meshlet_instances[meshlet_instance_index];
    let mesh = params.meshes[meshlet_instance.mesh_index];
    let transform = params.transforms[meshlet_instance.transform_index];
    let meshlet = mesh.meshlets[meshlet_instance.meshlet_index];

    let index = meshlet.index(mesh, vis.triangle_index);
    let vertex = meshlet.vertex(mesh, index);
    let vertex_pos = meshlet.position(mesh, vertex);
    let tex_coord = meshlet.tex_coord(mesh, vertex);
    let world_pos = transform.to_world_position(vertex_pos);
    let clip_pos = mul(params.camera.projection_view_mat, f32x4(world_pos.xyz, 1.0));

    VertexOutput output;
    output.position = clip_pos;
    output.vertex_pos = vertex_pos;
    output.tex_coord = tex_coord;
    output.meshlet_instance_index = vis.meshlet_instance_index;
    output.triangle_index = vis.triangle_index / 3;
    output.material_index = meshlet_instance.material_index;

    return output;
}

[[shader("fragment")]]
func fs_main(VertexOutput input) -> u32 {
    let material = params.materials[input.material_index];
    if (material.albedo_image_index != ~0u) {
        UVGradient grad;
        grad.uv = input.tex_coord;
        grad.ddx = ddx(input.tex_coord);
        grad.ddy = ddy(input.tex_coord);
        const f32 alpha_color = material.sample_albedo_color(grad).a;

        // We are doing deferred, blend alpha mode is not supported in this pass.
        if (alpha_color < clamp(material.alpha_cutoff, 0.001, 1.0) /* &&
            material.alpha_mode == AlphaMode::Mask*/)
        {
            discard;
        }
    }

    std::atomic_add(params.overdraw[u32x2(input.position.xy)], 1u, std::memory_order_acq_rel, std::MemoryLocation::Image, MemoryScope::QueueFamily);

    const let vis = VisBufferData(input.meshlet_instance_index, input.triangle_index);
    return vis.encode();
}
