module scene;

import std;
import gpu;

public const static f32 CAMERA_SCALE_UNIT = 0.01;
public const static f32 INV_CAMERA_SCALE_UNIT = 1.0 / CAMERA_SCALE_UNIT;
public const static f32 PLANET_RADIUS_OFFSET = 0.001;

public enum DebugView : i32 {
    None = 0,
    Triangles,
    Meshlets,
    Overdraw,
    Albedo,
    Normal,
    Emissive,
    Metallic,
    Roughness,
    Occlusion,
};

[[Flags]]
public enum CullFlags : u32 {
    MeshFrustum,
    MeshOcclusion,
    MeshletFrustum,
    MeshletOcclusion,
    TriangleBackFace,
    MicroTriangles,
};

public struct Camera {
    public mat4 projection_mat;
    public mat4 inv_projection_mat;
    public mat4 view_mat;
    public mat4 inv_view_mat;
    public mat4 projection_view_mat;
    public mat4 inv_projection_view_mat;
    public f32x3 position;
    public f32x2 resolution;
    public f32 near_clip;
    public f32 far_clip;
    public f32 acceptable_lod_error;
    public f32 fov_deg;
    public f32 aspect_ratio;

    public func unproject_uv(f32x2 uv, f32 depth) -> f32x3 {
        let ndc = f32x4(uv * 2.0 - 1.0, depth, 1.0);
        let world_pos_h = mul(this.inv_projection_view_mat, ndc);
        return world_pos_h.xyz / world_pos_h.w;
    }
};

public struct VirtualClipmapInfo {
    public f32x2 wrapped_vsm_uv;
    public i32x2 page_address;
    public f32x3 light_pos;
    public f32 projected_depth;
};

public struct VirtualClipmap {
    public f32x4x4 projection_view_mat;
    public i32x2 page_offset;

    public func get_clipmap_info(f32x3 world_pos, i32x2 page_table_extent) -> VirtualClipmapInfo {
        let light_pos_h = mul(this.projection_view_mat, f32x4(world_pos, 1.0));
        let projected_depth = light_pos_h.z / light_pos_h.w;
        let light_pos = light_pos_h.xyz / light_pos_h.w;
        let wrapped_uv = fract(light_pos.xy * 0.5 + 0.5);
        let page_address = i32x2(wrapped_uv * f32x2(page_table_extent));

        return VirtualClipmapInfo(wrapped_uv, page_address, light_pos, projected_depth);
    }
};

public struct DirectionalLight {
    public f32x3 base_ambient_color;
    public f32 intensity;
    public f32x3 direction;
    public u32 clipmap_count;
    public f32 first_clipmap_width;
    public f32 depth_bias;
    public f32 normal_bias;

    public func get_clipmap_index(in Camera camera, f32x2 uv, f32 depth) -> u32 {
#if 1
        // 2x2 diagonal pixel footprint calculation for better spatial information.
        // if this shit is too slow fallback to 2x1 approach (top down or left right)
        let inv_resolution = 1.0 / camera.resolution;
        let uv_offset = inv_resolution * 0.5;
        let top_left = uv + f32x2(uv_offset.x, uv_offset.y);
        let top_right = uv + f32x2(-uv_offset.x, uv_offset.y);
        let bottom_left = uv + f32x2(uv_offset.x, -uv_offset.y);
        let bottom_right = uv + f32x2(-uv_offset.x, uv_offset.y);
        let dx = distance(
            camera.unproject_uv(top_left, depth),
            camera.unproject_uv(bottom_right, depth)
        );
        let dy = distance(
            camera.unproject_uv(top_right, depth),
            camera.unproject_uv(bottom_left, depth)
        );
        let d = min(dx, dy);
#else
        let center_pos = camera.unproject_uv(uv, depth);
        let d = length(center_pos - camera.position);
#endif
        let bias = 1.0;
        let clipmap_length = this.first_clipmap_width / (camera.far_clip * 0.5);
        return u32(clamp(ceil(log2((d / clipmap_length) * bias)), 0.0, this.clipmap_count - 1.0));
    }
};

public struct Lights {
    public DirectionalLight *directional_light;
    public VirtualClipmap *directional_light_clipmaps;
};

public struct Atmosphere {
    public f32x3 sun_direction;
    public f32 eye_height;
    public f32x3 rayleigh_scatter;
    public f32 rayleigh_density;
    public f32x3 mie_scatter;
    public f32 mie_density;
    public f32 mie_extinction;
    public f32 mie_asymmetry;
    public f32x3 ozone_absorption;
    public f32 ozone_height;
    public f32 ozone_thickness;
    public f32x3 terrain_albedo;
    public f32 planet_radius;
    public f32 atmos_radius;
    public f32 aerial_perspective_start_km;
    public f32 sun_intensity;

    public i32x3 transmittance_lut_size;
    public i32x3 sky_view_lut_size;
    public i32x3 multiscattering_lut_size;
    public i32x3 aerial_perspective_lut_size;
};

public struct PBRContext {
    public Atmosphere *atmosphere;
    public DirectionalLight *directional_light;
    public VirtualClipmap *directional_light_clipmaps;
};

public struct EyeAdaptation {
    public f32 min_exposure;
    public f32 max_exposure;
    public f32 adaptation_speed;
    public f32 ISO_K;
};

public struct VBGTAO {
    public f32 thickness;
    public f32 depth_range_scale_factor;
    public f32 radius;
    public f32 radius_multiplier;
    public f32 slice_count;
    public f32 sample_count_per_slice;
    public f32 denoise_power;
    public f32 linear_thickness_multiplier;
};

public struct Transform {
    public mat4 local = {};
    public mat4 world = {};
    public mat3 normal = {};

    public func to_world_position(in f32x3 position) -> f32x4 {
        return mul(this.world, f32x4(position, 1.0));
    }

    public func to_world_normal(in f32x3 normal) -> f32x3 {
        return mul(this.normal, normal);
    }

    public func to_world_positions(in f32x3x3 positions) -> f32x4x3 {
        return {
            mul(this.world, f32x4(positions[0], 1.0)),
            mul(this.world, f32x4(positions[1], 1.0)),
            mul(this.world, f32x4(positions[2], 1.0)),
        };
    }

    public func to_world_normals(in f32x3x3 normals) -> f32x3x3 {
        return { mul(this.normal, normals[0]), mul(this.normal, normals[1]), mul(this.normal, normals[2]) };
    }
};

[[vk::binding(0, 1)]]
Sampler bindless_samplers[];
[[vk::binding(1, 1)]]
Image2D<f32x4> bindless_images[];
[[vk::binding(2, 1)]]
StorageImage2D<f32x4> bindless_storage_images[];

public enum MaterialFlag : u32 {
    None = 0,
    // Image flags
    HasAlbedoImage = 1 << 0,
    HasNormalImage = 1 << 1,
    HasEmissiveImage = 1 << 2,
    HasMetallicRoughnessImage = 1 << 3,
    HasOcclusionImage = 1 << 4,
    // Normal flags
    NormalTwoComponent = 1 << 5,
    NormalFlipY = 1 << 6,
    // Alpha
    AlphaOpaque = 1 << 7,
    AlphaMask = 1 << 8,
    AlphaBlend = 1 << 9,
};

public struct Material {
    public f32x4 albedo_color = {};
    public f32x3 emissive_color = {};
    public f32 roughness_factor = 0.0;
    public f32 metallic_factor = 0.0;
    public f32 alpha_cutoff = 0.0;
    public MaterialFlag flags = MaterialFlag::None;
    public u32 sampler_index = 0;
    public u32 albedo_image_index = 0;
    public u32 normal_image_index = 0;
    public u32 emissive_image_index = 0;
    public u32 metallic_roughness_image_index = 0;
    public u32 occlusion_image_index = 0;

    public func sample_albedo_color(f32x2 uv, f32x2 ddx, f32x2 ddy) -> f32x4 {
        if (this.flags & MaterialFlag::HasAlbedoImage) {
            let color = bindless_images[this.albedo_image_index]
                .SampleGrad(bindless_samplers[this.sampler_index], uv, ddx, ddy);
            return this.albedo_color * color;
        }

        return this.albedo_color;
    }

    public func sample_normal_color(f32x2 uv, f32x2 ddx, f32x2 ddy) -> f32x3 {
        return bindless_images[this.normal_image_index]
            .SampleGrad(bindless_samplers[this.sampler_index], uv, ddx, ddy).rgb;
    }

    public func sample_emissive_color(f32x2 uv, f32x2 ddx, f32x2 ddy) -> f32x3 {
        if (this.flags & MaterialFlag::HasEmissiveImage) {
            let color = bindless_images[this.emissive_image_index]
                .SampleGrad(bindless_samplers[this.sampler_index], uv, ddx, ddy).rgb;
            return this.emissive_color * color;
        }

        return this.emissive_color;
    }

    public func sample_metallic_roughness(f32x2 uv, f32x2 ddx, f32x2 ddy) -> f32x2 {
        let metallic_roughness = f32x2(this.metallic_factor, this.roughness_factor);
        if (this.flags & MaterialFlag::HasMetallicRoughnessImage) {
            let color = bindless_images[this.metallic_roughness_image_index]
                .SampleGrad(bindless_samplers[this.sampler_index], uv, ddx, ddy).bg;
            return metallic_roughness * color;
        }

        return metallic_roughness;
    }

    public func sample_occlusion_color(f32x2 uv, f32x2 ddx, f32x2 ddy) -> f32 {
        if (this.flags & MaterialFlag::HasOcclusionImage) {
            return bindless_images[this.occlusion_image_index]
                .SampleGrad(bindless_samplers[this.sampler_index], uv, ddx, ddy).r;
        }

        return 1.0;
    }
};

public struct Meshlet {
    public u32 indirect_vertex_index_offset = 0;
    public u32 local_triangle_index_offset = 0;
    public u32 vertex_count = 0;
    public u32 triangle_count = 0;

    // Takes a local triange index and returns an index to index buffer.
    public func index(in MeshLOD mesh_lod, u32 i) -> u32 {
        let local_triangle_index = u32(mesh_lod.local_triangle_indices[this.local_triangle_index_offset + i]);
        return mesh_lod.indirect_vertex_indices[this.indirect_vertex_index_offset + local_triangle_index];
    }

    // Returns position of a vertex.
    public func position(in Mesh mesh, u32 index) -> f32x3 {
        return mesh.vertex_positions[index];
    }

    public func tex_coord(in Mesh mesh, u32 index) -> f32x2 {
        if (mesh.texture_coords == nullptr) {
            return {};
        }

        return mesh.texture_coords[index];
    }

    // ----------------------------------------------------------

    public func indices(in MeshLOD mesh_lod, u32 i) -> u32x3 {
        let local_triangle_indices = u32x3(
            u32(mesh_lod.local_triangle_indices[this.local_triangle_index_offset + i * 3 + 0]),
            u32(mesh_lod.local_triangle_indices[this.local_triangle_index_offset + i * 3 + 1]),
            u32(mesh_lod.local_triangle_indices[this.local_triangle_index_offset + i * 3 + 2]),
        );

        return { mesh_lod.indirect_vertex_indices[this.indirect_vertex_index_offset + local_triangle_indices.x],
                 mesh_lod.indirect_vertex_indices[this.indirect_vertex_index_offset + local_triangle_indices.y],
                 mesh_lod.indirect_vertex_indices[this.indirect_vertex_index_offset + local_triangle_indices.z] };
    }

    public func positions(in Mesh mesh, in u32x3 indices) -> f32x3x3 {
        return { mesh.vertex_positions[indices.x],
                 mesh.vertex_positions[indices.y],
                 mesh.vertex_positions[indices.z] };
    }

    public func normals(in Mesh mesh, in u32x3 indices) -> f32x3x3 {
        return { mesh.vertex_normals[indices.x],
                 mesh.vertex_normals[indices.y],
                 mesh.vertex_normals[indices.z] };
    }

    public func tex_coords(in Mesh mesh, in u32x3 indices) -> f32x2x3 {
        if (mesh.texture_coords == nullptr) {
            return {};
        }

        return { mesh.texture_coords[indices.x],
                 mesh.texture_coords[indices.y],
                 mesh.texture_coords[indices.z] };
    }
};

public struct Bounds {
    public f32x3 aabb_center = {};
    public f32x3 aabb_extent = {};
    public f32x3 sphere_center = {};
    public f32 sphere_radius = 0.0f;
};

public struct MeshletInstance {
    public u32 mesh_instance_index = 0;
    public u32 meshlet_index = 0;
};

public struct MeshInstance {
    public u32 mesh_index = 0;
    public u32 lod_index = 0;
    public u32 material_index = 0;
    public u32 transform_index = 0;
    public u32 meshlet_instance_visibility_offset = 0;
};

public struct MeshLOD {
    public u32 *indices = nullptr;
    public Meshlet *meshlets = nullptr;
    public Bounds *meshlet_bounds = nullptr;
    public u8 *local_triangle_indices = nullptr;
    public u32 *indirect_vertex_indices = nullptr;
    public u32 indices_count = 0;
    public u32 meshlet_count = 0;
    public u32 meshlet_bounds_count = 0;
    public u32 local_triangle_indices_count = 0;
    public u32 indirect_vertex_indices_count = 0;
    public f32 error = 0.0;
};

#ifndef MESH_MAX_LODS
#define MESH_MAX_LODS 8
#endif

public struct Mesh {
    public f32x3 *vertex_positions = nullptr;
    public f32x3 *vertex_normals = nullptr;
    public f32x2 *texture_coords = nullptr;
    public u32 vertex_count = 0;
    public u32 lod_count = 0;
    public MeshLOD lods[MESH_MAX_LODS] = {};
    public Bounds bounds = {};
};

public struct MeshletInstanceVisibility {
    public u32 total_visible_meshlet_instances;
    public u32 early_visible_meshlet_instances;
    public u32 late_visible_meshlet_instances;
};
