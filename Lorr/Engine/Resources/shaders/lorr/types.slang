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

public struct SampledImage<Format> {
    ImageViewID image_view_id : 24;
    SamplerID sampler_id : 8;

    [ForceInline]
    public __init(ImageViewID image_view_id, SamplerID sampler_id) {
        this.image_view_id = image_view_id;
        this.sampler_id = sampler_id;
    }
};

public struct DispatchIndirectCommand {
    public u32 x;
    public u32 y;
    public u32 z;
};

// Direct replica of vulkan's indirect indexed struct
public struct DrawIndexedIndirectCommand {
    public u32 index_count;
    public u32 instance_count;
    public u32 first_index;
    public i32 vertex_offset;
    public u32 first_instance;
};
