#ifdef __cplusplus
#pragma once
#endif

#define LR_DESCRIPTOR_INDEX_SAMPLER 0
#define LR_DESCRIPTOR_INDEX_IMAGES 1
#define LR_DESCRIPTOR_INDEX_STORAGE_IMAGES 2
#define LR_DESCRIPTOR_INDEX_STORAGE_BUFFERS 3
#define LR_DESCRIPTOR_INDEX_BDA_ARRAY 4

#ifdef __cplusplus
// C++ stuff goes here
#else
// Slang stuff goes here

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
}

#endif

