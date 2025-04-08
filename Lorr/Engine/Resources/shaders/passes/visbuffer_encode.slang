module visbuffer_encode;

import std;
import gpu;
import scene;
import visbuffer;

struct PushConstants {
    Camera *camera;
    u32 *visible_meshlet_instances_indices;
    MeshletInstance *meshlet_instances;
    Model *models;
    Transform *transforms;
    Material *materials;
};
[[vk::push_constant]] PushConstants C;

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
    const VisBuffer vis = { vertex_index };
    const u32 meshlet_instance_index = C.visible_meshlet_instances_indices[vis.meshlet_instance_index];
    const MeshletInstance *meshlet_instance = &C.meshlet_instances[meshlet_instance_index];
    const Model *model = &C.models[meshlet_instance->model_index];
    const Transform *transform = &C.transforms[meshlet_instance->transform_index];
    const Meshlet *meshlet = &model->meshlets[meshlet_instance->meshlet_index];

    const u32 index  = meshlet->index(model, vis.triangle_index);
    const u32 vertex = meshlet->vertex(model, index);
    const f32x3 vertex_pos = meshlet->position(model, vertex);
    const f32x2 tex_coord  = meshlet->tex_coord(model, vertex);
    const f32x4 world_pos  = transform->to_world_position(vertex_pos);
    const f32x4 clip_pos   = C.camera->to_clip_position(world_pos);

    VertexOutput output;
    output.position = clip_pos;
    output.vertex_pos = vertex_pos;
    output.tex_coord = tex_coord;
    output.meshlet_instance_index = vis.meshlet_instance_index;
    output.triangle_index = vis.triangle_index / 3;
    output.material_index = meshlet_instance->material_index;

    return output;
}

[[shader("fragment")]]
func fs_main(VertexOutput input) -> u32 {
    const Material material = C.materials[input.material_index];
    if (material.albedo_image_index != ~0u) {
        const f32x2 grad_x = ddx(input.tex_coord);
        const f32x2 grad_y = ddy(input.tex_coord);
        const f32 alpha_color = material.sample_albedo_color(
            input.tex_coord, grad_x, grad_y).a;
        if (alpha_color < clamp(material.alpha_cutoff, 0.001, 1.0)) {
            discard;
        }
    }

    const VisBuffer vis = { input.meshlet_instance_index, input.triangle_index };
    return vis.encode();
}
