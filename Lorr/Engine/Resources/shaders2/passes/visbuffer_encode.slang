module visbuffer_encode;

import std;
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
    nointerpolation u32 vis_data : VIS_DATA;
    nointerpolation u32 material_index : MATERIAL_INDEX;
};

[[shader("vertex")]]
func vs_main(u32 vertex_index : SV_VertexID) -> VertexOutput {
    const VisBuffer vis = {vertex_index};
    const u32 visible_meshlet_instance_index = C.visible_meshlet_instances_indices[vis.meshlet_index];
    const MeshletInstance *meshlet_instance = &C.meshlet_instances[visible_meshlet_instance_index];
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
    output.vis_data = vis.encode();
    output.material_index = meshlet_instance->material_index;
}

[[shader("fragment")]]
func fs_main(VertexOutput input) -> u32 {
    const Material material = C.materials[input.material_index];
    if (material.albedo_image.is_valid()) {
        const f32x2 grad_x  = ddx(input.tex_coord);
        const f32x2 grad_y  = ddy(input.tex_coord);
        const f32 alpha_col = material.sample_albedo_color(input.tex_coord, grad_x, grad_y).a;
        if (alpha_col < clamp(material.alpha_cutoff, 0.001, 1.0)) {
            discard;
        }
    }
    return input.vis_data;
}