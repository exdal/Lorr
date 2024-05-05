#ifdef __cplusplus
#pragma once
#endif

#define LR_DESCRIPTOR_INDEX_SAMPLER 0
#define LR_DESCRIPTOR_INDEX_IMAGES 1
#define LR_DESCRIPTOR_INDEX_STORAGE_IMAGES 2
#define LR_DESCRIPTOR_INDEX_BUFFERS 3

#ifdef __cplusplus
constexpr static const char *LR_SHADER_STD_FILE_PATH = LR_PROJECT_DIR "/Lorr/Engine/Graphics/lorr.slang";

#else

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

/// TODO: About storage images, RWTexture2D can have multiple formats
/// while staying at the same binding index.
namespace lr {
[[vk::binding(LR_DESCRIPTOR_INDEX_SAMPLER, 0)]]
Sampler2D __samplers[];
extension Sampler2D {
  static Sampler2D get(u32 i) { return __samplers[i]; }
}

[[vk::binding(LR_DESCRIPTOR_INDEX_IMAGES, 0)]]
Texture2D __textures[];
extension Texture2D {
  static Texture2D get(u32 i) { return __textures[i]; }
};

[[vk::binding(LR_DESCRIPTOR_INDEX_STORAGE_IMAGES, 0)]]
RWTexture2D<float4> __storage_images[];
extension RWTexture2D<float4> {
  static RWTexture2D<float4> get(u32 i) { return __storage_images[i]; }
};

[[vk::binding(LR_DESCRIPTOR_INDEX_BUFFERS, 0)]]
ByteAddressBuffer __buffers[];
extension ByteAddressBuffer {
  static ByteAddressBuffer get(u32 i) { return __buffers[i]; };
};

}


#endif
