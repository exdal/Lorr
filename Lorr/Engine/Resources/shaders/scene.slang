module scene;

import std;
import gpu;

public const static f32 CAMERA_SCALE_UNIT     = 0.01;
public const static f32 INV_CAMERA_SCALE_UNIT = 1.0 / CAMERA_SCALE_UNIT;
public const static f32 PLANET_RADIUS_OFFSET  = 0.001;

[[Flags]]
public enum CullFlags : u32 {
    MeshletFrustum,
    TriangleBackFace,
    MicroTriangles,
};

public struct Sun {
    public f32x3 direction;
    public f32   intensity;
};

public struct Atmosphere {
    public f32x3 rayleigh_scatter;
    public f32   rayleigh_density;

    public f32x3 mie_scatter;
    public f32   mie_density;
    public f32   mie_extinction;
    public f32   mie_asymmetry;

    public f32x3 ozone_absorption;
    public f32   ozone_height;
    public f32   ozone_thickness;

    public f32x3 terrain_albedo;
    public f32   aerial_gain_per_slice;
    public f32   planet_radius;
    public f32   atmos_radius;

    public i32x3 transmittance_lut_size;
    public i32x3 sky_view_lut_size;
    public i32x3 multiscattering_lut_size;
    public i32x3 aerial_perspective_lut_size;
};

public struct Camera {
    public mat4  projection_mat;
    public mat4  view_mat;
    public mat4  projection_view_mat;
    public mat4  inv_view_mat;
    public mat4  inv_projection_view_mat;
    public mat4  frustum_projection_view_mat;
    public f32x3 position;
    public f32   near_clip;
    public f32   far_clip;
    public f32x4 frustum_planes[6];
    public f32x2 resolution;

    public func to_clip_position(in f32x4 world_position) -> f32x4 {
        return mul(this.projection_view_mat, world_position);
    }

    public func to_clip_positions(in f32x4x3 world_positions) -> f32x4x3 {
        return {
            mul(this.projection_view_mat, world_positions[0]),
            mul(this.projection_view_mat, world_positions[1]),
            mul(this.projection_view_mat, world_positions[2]),
        };
    }
};

public struct Transform {
    public mat4 local = {};
    public mat4 world = {};
    public mat3 normal = {};

    public func to_world_position(in f32x3 position) -> f32x4 {
        return mul(this.world, f32x4(position, 1.0));
    }

    public func to_world_positions(in f32x3x3 positions) -> f32x4x3 {
        return {
            mul(this.world, f32x4(positions[0], 1.0)),
            mul(this.world, f32x4(positions[1], 1.0)),
            mul(this.world, f32x4(positions[2], 1.0)),
        };
    }
};

public enum AlphaMode : u32 {
    Opaque = 0,
    Mask,
    Blend,
};

public struct Material {
    public f32x4 albedo_color = {};
    public f32x3 emissive_color = {};
    public f32   roughness_factor = 0.0;
    public f32   metallic_factor = 0.0;
    public AlphaMode alpha_mode = AlphaMode::Opaque;
    public f32   alpha_cutoff = 0.0;
    public Image2D<f32x4> albedo_image;
    public Image2D<f32x4> normal_image;
    public Image2D<f32x4> emissive_image;
    public Image2D<f32x4> metallic_rougness_image;
    public Image2D<f32x4> occlusion_image;

    public func sample_albedo_color(in Sampler sampler, f32x2 uv, f32x2 ddx, f32x2 ddy) -> f32x4 {
        if (this.albedo_image.is_valid()) {
            const let color = this.albedo_image.sample_grad(sampler, uv, ddx, ddy);
            return this.albedo_color * color;
        }

        return this.albedo_color;
    }

    public func sample_normal_color(in Sampler sampler, f32x2 uv, f32x2 ddx, f32x2 ddy) -> f32x3 {
        if (this.normal_image.is_valid()) {
            return this.normal_image.sample_grad(sampler, uv, ddx, ddy).rgb;
        }

        return { 0.0 };
    }

    public func sample_emissive_color(in Sampler sampler, f32x2 uv, f32x2 ddx, f32x2 ddy) -> f32x3 {
        if (this.emissive_image.is_valid()) {
            const let color = this.emissive_image.sample_grad(sampler, uv, ddx, ddy).rgb;
            return this.emissive_color * color;
        }

        return this.emissive_color;
    }

    public func sample_metallic_roughness(in Sampler sampler, f32x2 uv, f32x2 ddx, f32x2 ddy) -> f32x2 {
        const let metallic_roughness = f32x2(this.metallic_factor, this.roughness_factor);
        if (this.metallic_rougness_image.is_valid()) {
            const let color = this.metallic_rougness_image.sample_grad(sampler, uv, ddx, ddy).bg;
            return metallic_roughness * color;
        }

        return metallic_roughness;
    }

    public func sample_occlusion_color(in Sampler sampler, f32x2 uv, f32x2 ddx, f32x2 ddy) -> f32 {
        if (this.occlusion_image.is_valid()) {
            return this.occlusion_image.sample_grad(sampler, uv, ddx, ddy).r;
        }

        return 1.0;
    }
};

public typealias Triangle = u32x3;
public struct Meshlet {
    public u32 vertex_offset = 0;
    public u32 index_offset = 0;
    public u32 triangle_offset = 0;
    public u32 triangle_count = 0;

    // Takes a local triange index and returns an index to index buffer.
    public func index(Model *model, u32 i) -> u32 {
        return u32(model->local_triangle_indices[this.triangle_offset + i]);
    }

    // Returns index to an actual vertex.
    public func vertex(Model *model, u32 index) -> u32 {
        return model->indices[this.index_offset + index];
    }

    // Returns position of a vertex.
    public func position(Model *model, u32 vertex) -> f32x3 {
        return model->vertex_positions[this.vertex_offset + vertex];
    }

    public func tex_coord(Model *model, u32 vertex) -> f32x2 {
        return model->texture_coords[this.vertex_offset + vertex];
    }

    // ----------------------------------------------------------

    public func triangle(Model *model, u32 i) -> Triangle {
        return {
            u32(model->local_triangle_indices[this.triangle_offset + i * 3 + 0]),
            u32(model->local_triangle_indices[this.triangle_offset + i * 3 + 1]),
            u32(model->local_triangle_indices[this.triangle_offset + i * 3 + 2]),
        };
    }

    public func indices(Model *model, in Triangle triangle) -> u32x3 {
        return {
            model->indices[this.index_offset + triangle.x],
            model->indices[this.index_offset + triangle.y],
            model->indices[this.index_offset + triangle.z],
        };
    }

    public func positions(Model *model, in u32x3 indices) -> f32x3x3 {
        return {
            model->vertex_positions[this.vertex_offset + indices.x],
            model->vertex_positions[this.vertex_offset + indices.y],
            model->vertex_positions[this.vertex_offset + indices.z],
        };
    }

    public func normals(Model *model, in u32x3 indices) -> f32x3x3 {
        return {
            model->vertex_normals[this.vertex_offset + indices.x],
            model->vertex_normals[this.vertex_offset + indices.y],
            model->vertex_normals[this.vertex_offset + indices.z]
        };
    }

    public func tex_coords(Model *model, in u32x3 indices) -> f32x2x3 {
        if (model->texture_coords == nullptr) {
            return {};
        }

        return {
            model->texture_coords[this.vertex_offset + indices.x],
            model->texture_coords[this.vertex_offset + indices.y],
            model->texture_coords[this.vertex_offset + indices.z],
        };
    }
};

public struct MeshletBounds {
    public f32x3 aabb_min = {};
    public f32x3 aabb_max = {};
};

public struct MeshletInstance {
    public u32 model_index = 0;
    public u32 material_index = 0;
    public u32 transform_index = 0;
    public u32 meshlet_index = 0;
};

public struct Model {
    public u32 *indices = nullptr;
    public f32x3 *vertex_positions = nullptr;
    public f32x3 *vertex_normals = nullptr;
    public f32x2 *texture_coords = nullptr;
    public Meshlet *meshlets = nullptr;
    public MeshletBounds *meshlet_bounds = nullptr;
    public u8 *local_triangle_indices = nullptr;
};

public struct Light {
    public f32x3 position = {};
    public f32x3 color = {};
};
