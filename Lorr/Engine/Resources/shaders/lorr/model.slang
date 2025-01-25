implementing "../lorr";

public struct Vertex {
    public float3 position;
    public float3 normal;
    public float2 tex_coord;
    public u32 color;
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
    public mat4 transform_mat;
    public u32 material_index;
};

