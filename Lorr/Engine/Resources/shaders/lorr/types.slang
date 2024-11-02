implementing "../lorr";

public static const float PI = 3.14159265358;

// arithmetic types
public typedef double f64;
public typedef float f32;
public typedef uint64_t u64;
public typedef int64_t i64;
public typedef uint32_t u32;
public typedef int32_t i32;
public typedef uint16_t u16;
public typedef int16_t i16;
public typedef uint8_t u8;
public typedef int8_t i8;
public typedef u32 b32;

// math
public typedef float2 vec2;
public typedef float3 vec3;
public typedef float4 vec4;
public typedef int2 ivec2;
public typedef int3 ivec3;
public typedef int4 ivec4;
public typedef uint2 uvec2;
public typedef uint3 uvec3;
public typedef uint4 uvec4;
public typedef matrix<f32, 4, 4> mat4;
public typedef matrix<f32, 4, 3> mat3;

// resource types
public typedef u32 BufferID;
public typedef u32 ImageID;
public typedef u32 ImageViewID;
public typedef u32 SamplerID;
public static u32 INVALID = ~0u;

#define LR_DESCRIPTOR_INDEX_SAMPLER 0
#define LR_DESCRIPTOR_INDEX_IMAGES 1
#define LR_DESCRIPTOR_INDEX_STORAGE_IMAGES 2
#define LR_DESCRIPTOR_INDEX_STORAGE_BUFFERS 3
#define LR_DESCRIPTOR_INDEX_BDA_ARRAY 4

[[vk::binding(LR_DESCRIPTOR_INDEX_SAMPLER, 0)]]         SamplerState __samplers[];
[[vk::binding(LR_DESCRIPTOR_INDEX_IMAGES, 0)]]          Texture2D<float4> __textures_f4[];
[[vk::binding(LR_DESCRIPTOR_INDEX_IMAGES, 0)]]          Texture3D<float4> __textures_f4_3d[];
[[vk::binding(LR_DESCRIPTOR_INDEX_STORAGE_IMAGES, 0)]]  RWTexture2D<float4> __storage_images[];
[[vk::binding(LR_DESCRIPTOR_INDEX_STORAGE_IMAGES, 0)]]  RWTexture3D<float4> __storage_images_3d[];
[[vk::binding(LR_DESCRIPTOR_INDEX_STORAGE_BUFFERS, 0)]] ByteAddressBuffer __storage_buffers[];
[[vk::binding(LR_DESCRIPTOR_INDEX_BDA_ARRAY, 0)]]       StructuredBuffer<u64> __bda_array;

public struct ImageView {
    ImageViewID handle;

    [ForceInline]
    public __init(ImageViewID id) {
        this.handle = id;
    }

    [ForceInline]
    public float4 sample(SamplerID sampler_id, float2 uv) {
        return __textures_f4[this.handle].Sample(__samplers[sampler_id], uv);
    }

    [ForceInline]
    public float4 sample(SamplerID sampler_id, float3 uvw) {
        return __textures_f4_3d[this.handle].Sample(__samplers[sampler_id], uvw);
    }

    public RWTexture2D rw() {
        return __storage_images[this.handle];
    }

    public RWTexture3D rw_3d() {
        return __storage_images_3d[this.handle];
    }
};

