implementing "../lorr";

public struct Transform {
    public mat4 local = {};
};

public struct Material {
    public vec3 albedo_color = {};
    public vec3 emissive_color = {};
    public f32 roughness_factor = 0.0;
    public f32 metallic_factor = 0.0;
    public u32 alpha_mode = 0;
    public f32 alpha_cutoff = 0.0;
    public SampledImage<float4> albedo_image;
    public SampledImage<float4> normal_image;
    public SampledImage<float4> emissive_image;
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
    public u32 model_index;
    public u32 material_index;
    public u32 transform_index;
    public u32 meshlet_index;
};

public struct Model {
    public float3 *vertex_positions;
    public u32 *indices;
    public Meshlet *meshlets;
    public MeshletBounds *meshlet_bounds;
    public u8 *local_triangle_indices;
};

