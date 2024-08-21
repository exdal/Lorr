#define LR_DESCRIPTOR_INDEX_SAMPLER 0
#define LR_DESCRIPTOR_INDEX_IMAGES 1
#define LR_DESCRIPTOR_INDEX_STORAGE_IMAGES 2
#define LR_DESCRIPTOR_INDEX_STORAGE_BUFFERS 3
#define LR_DESCRIPTOR_INDEX_BDA_ARRAY 4

static const float PI = 3.14159265358;

// arithmetic types
typedef double f64;
typedef float f32;
typedef uint64_t u64;
typedef int64_t i64;
typedef uint32_t u32;
typedef int32_t i32;
typedef uint16_t u16;
typedef int16_t i16;
typedef uint8_t u8;
typedef int8_t i8;
typedef u32 b32;

// resource types
typedef u32 BufferID;
typedef u32 ImageID;
typedef u32 ImageViewID;
typedef u32 SamplerID;
static u32 INVALID = ~0u;

/// TODO: About storage images, RWTexture2D can have multiple formats
/// while staying at the same binding index.
namespace lr {
[[vk::binding(LR_DESCRIPTOR_INDEX_SAMPLER, 0)]]
SamplerState __samplers[];
extension SamplerState {
  static SamplerState get(SamplerID i) { return __samplers[i]; }
}

[[vk::binding(LR_DESCRIPTOR_INDEX_IMAGES, 0)]]
Texture2D<float4> __textures[];
extension Texture2D<float4> {
  static Texture2D get(ImageViewID i) { return __textures[i]; }
};

[[vk::binding(LR_DESCRIPTOR_INDEX_STORAGE_IMAGES, 0)]]
RWTexture2D<float4> __storage_images[];
extension RWTexture2D<float4> {
  static RWTexture2D<float4> get(ImageID i) { return __storage_images[i]; }
};

[[vk::binding(LR_DESCRIPTOR_INDEX_STORAGE_BUFFERS, 0)]]
ByteAddressBuffer __storage_buffers[];
extension ByteAddressBuffer {
  static ByteAddressBuffer get(BufferID i) { return __storage_buffers[i]; };
};

[[vk::binding(LR_DESCRIPTOR_INDEX_BDA_ARRAY, 0)]]
StructuredBuffer<u64> __bda_array;
func load_buffer<T>(BufferID buffer_id) -> T * {
    return (T *)(__bda_array[buffer_id]);
}

struct Material {
    float4 albedo_color = { 1.0f, 1.0f, 1.0f, 1.0f };
    float4 emissive_color = { 0.0f, 0.0f, 0.0f, 1.0f };
    f32 roughness_factor = 0.0f;
    f32 metallic_factor = 0.0f;
    u32 alpha_mode = 0u;
    f32 alpha_cutoff = 0.0f;
    ImageViewID albedo_image_view = INVALID;
    SamplerID albedo_sampler = INVALID;
    ImageViewID normal_image_view = INVALID;
    SamplerID normal_sampler = INVALID;
    ImageViewID emissive_image_view = INVALID;
    SamplerID emissive_sampler = INVALID;
};

struct Sun {
    float3 direction;
    f32 intensity;
};

struct Atmosphere {
    float3 rayleigh_scatter;
    float rayleigh_density;
    float planet_radius;
    float atmos_radius;
    float mie_scatter;
    float mie_absorption;
    float mie_density;
    float mie_asymmetry;
    float ozone_height;
    float ozone_thickness;
    float3 ozone_absorption;
};

struct Camera {
    float4x4 projection_mat;
    float4x4 view_mat;
    float4x4 projection_view_mat;
    float4x4 inv_view_mat;
    float4x4 inv_projection_view_mat;
    float3 position;
};

struct World {
    Camera *cameras;
    Material *materials;
    Sun sun;
    Atmosphere atmosphere;
};

extension World {
    static World *from_buffer(BufferID buffer_id) {
        return load_buffer<World>(buffer_id);
    }
};

}

