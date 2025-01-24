implementing "../lorr";

public static const float PI = 3.1415926535897932384626433832795;
public static const float TAU = 6.283185307179586476925286766559;

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
public typedef u32 ImageViewID;
public typedef u32 SamplerID;

#ifdef LR_BINDLESS_PIPELINE

#define LR_DESCRIPTOR_INDEX_SAMPLER 0
#define LR_DESCRIPTOR_INDEX_IMAGES 1
#define LR_DESCRIPTOR_INDEX_STORAGE_IMAGES 2
#define LR_DESCRIPTOR_INDEX_STORAGE_BUFFERS 3
#define LR_DESCRIPTOR_INDEX_BDA_ARRAY 4

[[vk::binding(LR_DESCRIPTOR_INDEX_SAMPLER, 0)]]         SamplerState __samplers[];
[[vk::binding(LR_DESCRIPTOR_INDEX_IMAGES, 0)]]          __DynamicResource __textures[];
[[vk::binding(LR_DESCRIPTOR_INDEX_STORAGE_IMAGES, 0)]]  __DynamicResource __storage_images[];
[[vk::binding(LR_DESCRIPTOR_INDEX_STORAGE_BUFFERS, 0)]] ByteAddressBuffer __storage_buffers[];
[[vk::binding(LR_DESCRIPTOR_INDEX_BDA_ARRAY, 0)]]       StructuredBuffer<u64> __bda_array;

public struct ImageView<Format> {
    ImageViewID handle;

    [ForceInline]
    public __init(ImageViewID id) {
        this.handle = id;
    }

    [ForceInline]
    public Format sample(SamplerID sampler_id, float2 uv) {
        return __textures[this.handle].as<Texture2D<Format>>().Sample(__samplers[sampler_id], uv);
    }

    [ForceInline]
    public Format sample(SamplerID sampler_id, float3 uvw) {
        return __textures[this.handle].as<Texture3D<Format>>().Sample(__samplers[sampler_id], uvw);
    }

    [ForceInline]
    public void store(uint2 thread_id, Format color) {
        const let rw_image = __storage_images[this.handle].as<RWTexture2D<Format>>();
        rw_image[thread_id] = color;
    }

    [ForceInline]
    public void store(uint3 thread_id, Format color) {
        const let rw_image = __storage_images[this.handle].as<RWTexture3D<Format>>();
        rw_image[thread_id] = color;
    }

    [ForceInline]
    public func id() -> ImageViewID {
        return this.handle;
    }
};
#endif

public struct SampledImage<Format> {
    ImageViewID image_view_id : 24;
    SamplerID sampler_id : 8;

    [ForceInline]
    public __init(ImageViewID image_view_id, SamplerID sampler_id) {
        this.image_view_id = image_view_id;
        this.sampler_id = sampler_id;
    }

#ifdef LR_BINDLESS_PIPELINE
    [ForceInline]
    public Format sample(float2 uv) {
        return __textures[this.image_view_id].as<Texture2D<Format>>().Sample(__samplers[this.sampler_id], uv);
    }

    [ForceInline]
    public Format sample(float3 uvw) {
        return __textures[this.image_view_id].as<Texture3D<Format>>().Sample(__samplers[this.sampler_id], uvw);
    }
#endif
};
