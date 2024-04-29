#pragma once

#define LR_DESCRIPTOR_INDEX_SAMPLER 0
#define LR_DESCRIPTOR_INDEX_IMAGES 1
#define LR_DESCRIPTOR_INDEX_STORAGE_IMAGES 2
#define LR_DESCRIPTOR_INDEX_DEVICE_ADDRESSES 3

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

// vector types
typedef float2 f32vec2;

namespace lr {
[[vk::binding(LR_DESCRIPTOR_INDEX_DEVICE_ADDRESSES, 0)]]
ByteAddressBuffer __device_addresses[];
extension ByteAddressBuffer {
  static ByteAddressBuffer get(u32 i) { return __device_addresses[i]; };
};

}

[[vk::binding(0, 0)]] SamplerState  u_samplers[];
[[vk::binding(1, 0)]] Texture2D     u_images[];
[[vk::binding(2, 0)]] RWTexture2D<u32>   u_storage_images_u32[];


/// TODO: This shader file can be directly imported from C++ source,
/// making it easier to manage between compilers.

/// TODO: About storage images, RWTexture2D can have multiple formats
/// while staying at the same binding index.

