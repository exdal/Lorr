implementing lorr;

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
[[vk::binding(LR_DESCRIPTOR_INDEX_STORAGE_IMAGES, 0)]]  RWTexture2D<float4> __storage_images[];
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
};

