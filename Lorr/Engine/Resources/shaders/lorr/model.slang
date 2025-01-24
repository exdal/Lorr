implementing "../lorr";

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

public struct MeshTransform {
    public mat4 transform_mat;
};

