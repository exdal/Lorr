implementing "../lorr";

public struct Meshlet {
    public u32 vertex_offset;
    public u32 index_offset;
    public u32 triangle_offset;
    public u32 triangle_count;
    public vec3 aabb_min;
    public vec3 aabb_max;
};

public struct MeshletInstance {
    public u32 meshlet_index;
    public u32 material_index;
};

public struct Material {
    public vec3 albedo_color;
    public vec3 emissive_color;
    public f32 roughness_factor;
    public f32 metallic_factor;
    public u32 alpha_mode;
    public f32 alpha_cutoff;
    public SampledImage<float4> albedo_image;
    public SampledImage<float4> normal_image;
    public SampledImage<float4> emissive_image;
};

public struct MeshInstance {
    public u32 entity_index;
    public u32 material_index;
};

