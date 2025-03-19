implementing "../lorr";

public struct Transform {
    public mat4 local = {};
    public mat4 world = {};
    public mat3 normal = {};
};

public struct Material {
    public vec4 albedo_color = {};
    public vec3 emissive_color = {};
    public f32 roughness_factor = 0.0;
    public f32 metallic_factor = 0.0;
    public u32 alpha_mode = 0;
    public f32 alpha_cutoff = 0.0;
    public u32 albedo_image_index = 0;
    public u32 normal_image_index = 0;
    public u32 emissive_image_index = 0;
    public u32 metallic_rougness_image_index = 0;
};

public struct Meshlet {
    public u32 vertex_offset;
    public u32 index_offset;
    public u32 triangle_offset;
    public u32 triangle_count;
};

public struct MeshletBounds {
    public vec3 aabb_min;
    public vec3 aabb_max;
};

public struct MeshletInstance {
    public u32 mesh_index;
    public u32 material_index;
    public u32 transform_index;
    public u32 meshlet_index;
};

public struct Model {
    public u32 *indices;
    public vec3 *vertex_positions;
    public vec3 *vertex_normals;
    public vec2 *texture_coords;
    public Meshlet *meshlets;
    public MeshletBounds *meshlet_bounds;
    public u8 *local_triangle_indices;
};

public struct Light {
    public vec3 position;
    public vec3 color;
};
