//
// Created on Monday 9th May 2022 by e-erdal
//

#pragma once

namespace lr
{
    enum APIType : u32
    {
        LR_API_TYPE_NULL,
        LR_API_TYPE_VULKAN,
        LR_API_TYPE_D3D12,
    };

    enum APIFlags : u32
    {
        LR_API_FLAG_NONE,
        LR_API_FLAG_VSYNC = 1 << 0,
    };
    EnumFlags(APIFlags);

    enum APIAllocatorType : u32
    {
        LR_API_ALLOCATOR_NONE,

        LR_API_ALLOCATOR_DESCRIPTOR,  // A linear, small sized pool with CPUW flag for per-frame descriptor data
        LR_API_ALLOCATOR_BUFFER_LINEAR,                             // A linear, medium sized pool for buffers
        LR_API_ALLOCATOR_BUFFER_TLSF,                               // Large sized pool for large scene buffers
        LR_API_ALLOCATOR_BUFFER_FRAMETIME = LR_API_ALLOCATOR_NONE,  //
                                                                    //
        LR_API_ALLOCATOR_IMAGE_TLSF,                                // Large sized pool for large images

        LR_API_ALLOCATOR_COUNT,
    };

    enum ResourceUsage : u32
    {
        LR_RESOURCE_USAGE_UNKNOWN = 0,
        LR_RESOURCE_USAGE_VERTEX_BUFFER = 1 << 0,
        LR_RESOURCE_USAGE_INDEX_BUFFER = 1 << 1,
        LR_RESOURCE_USAGE_CONSTANT_BUFFER = 1 << 3,
        LR_RESOURCE_USAGE_SHADER_RESOURCE = 1 << 4,
        LR_RESOURCE_USAGE_RENDER_TARGET = 1 << 5,
        LR_RESOURCE_USAGE_DEPTH_STENCIL = 1 << 6,
        LR_RESOURCE_USAGE_TRANSFER_SRC = 1 << 7,
        LR_RESOURCE_USAGE_TRANSFER_DST = 1 << 8,
        LR_RESOURCE_USAGE_UNORDERED_ACCESS = 1 << 9,
        LR_RESOURCE_USAGE_HOST_VISIBLE = 1 << 10,

        LR_RESOURCE_USAGE_PRESENT = LR_RESOURCE_USAGE_SHADER_RESOURCE | LR_RESOURCE_USAGE_RENDER_TARGET,

        LR_RESOURCE_USAGE_MAX = 1U << 31,
    };
    EnumFlags(ResourceUsage);

    enum ImageFormat : u32
    {
        LR_IMAGE_FORMAT_UNKNOWN,
        LR_IMAGE_FORMAT_RGBA8F,
        LR_IMAGE_FORMAT_RGBA8_SRGBF,
        LR_IMAGE_FORMAT_BGRA8F,
        LR_IMAGE_FORMAT_RGBA16F,
        LR_IMAGE_FORMAT_RGBA32F,
        LR_IMAGE_FORMAT_R32U,
        LR_IMAGE_FORMAT_R32F,
        LR_IMAGE_FORMAT_D32F,
        LR_IMAGE_FORMAT_D32FS8U,
    };

    enum ColorMask : u32
    {
        LR_COLOR_MASK_NONE = 0,
        LR_COLOR_MASK_R = 1 << 0,
        LR_COLOR_MASK_G = 1 << 1,
        LR_COLOR_MASK_B = 1 << 2,
        LR_COLOR_MASK_A = 1 << 3,
    };
    EnumFlags(ColorMask);

    enum PrimitiveType : u32
    {
        LR_PRIMITIVE_TYPE_POINT_LIST = 0,
        LR_PRIMITIVE_TYPE_LINE_LIST,
        LR_PRIMITIVE_TYPE_LINE_STRIP,
        LR_PRIMITIVE_TYPE_TRIANGLE_LIST,
        LR_PRIMITIVE_TYPE_TRIANGLE_STRIP,
        LR_PRIMITIVE_TYPE_PATCH,
    };

    enum CompareOp : u32
    {
        LR_COMPARE_OP_NEVER = 0,
        LR_COMPARE_OP_LESS,
        LR_COMPARE_OP_EQUAL,
        LR_COMPARE_OP_LESS_EQUAL,
        LR_COMPARE_OP_GREATER,
        LR_COMPARE_OP_NOT_EQUAL,
        LR_COMPARE_OP_GREATER_EQUAL,
        LR_COMPARE_OP_ALWAYS,
    };

    enum StencilOp : u32
    {
        LR_STENCIL_OP_KEEP = 0,
        LR_STENCIL_OP_ZERO,
        LR_STENCIL_OP_REPLACE,
        LR_STENCIL_OP_INCR_AND_CLAMP,
        LR_STENCIL_OP_DECR_AND_CLAMP,
        LR_STENCIL_OP_INVERT,
        LR_STENCIL_OP_INCR_AND_WRAP,
        LR_STENCIL_OP_DECR_AND_WRAP,
    };

    enum CullMode : u32
    {
        LR_CULL_MODE_NONE = 0,
        LR_CULL_MODE_FRONT,
        LR_CULL_MODE_BACK,
    };

    enum FillMode : u32
    {
        LR_FILL_MODE_FILL = 0,
        LR_FILL_MODE_WIREFRAME,
    };

    enum BlendFactor : u32
    {
        LR_BLEND_FACTOR_ZERO = 0,
        LR_BLEND_FACTOR_ONE,
        LR_BLEND_FACTOR_SRC_COLOR,
        LR_BLEND_FACTOR_INV_SRC_COLOR,
        LR_BLEND_FACTOR_SRC_ALPHA,
        LR_BLEND_FACTOR_INV_SRC_ALPHA,
        LR_BLEND_FACTOR_DST_ALPHA,
        LR_BLEND_FACTOR_INV_DST_ALPHA,
        LR_BLEND_FACTOR_DST_COLOR,
        LR_BLEND_FACTOR_INV_DST_COLOR,
        LR_BLEND_FACTOR_SRC_ALPHA_SAT,
        LR_BLEND_FACTOR_CONSTANT_FACTOR,
        LR_BLEND_FACTOR_INV_CONSTANT_FACTOR,
        LR_BLEND_FACTOR_SRC_1_COLOR,
        LR_BLEND_FACTOR_INV_SRC_1_COLOR,
        LR_BLEND_FACTOR_SRC_1_ALPHA,
        LR_BLEND_FACTOR_INV_SRC_1_ALPHA,
    };

    enum BlendOp : u32
    {
        LR_BLEND_OP_ADD = 0,
        LR_BLEND_OP_SUBTRACT,
        LR_BLEND_OP_REVERSE_SUBTRACT,
        LR_BLEND_OP_MIN,
        LR_BLEND_OP_MAX,
    };

    enum Filtering : u32
    {
        LR_FILTERING_NEAREST = 0,
        LR_FILTERING_LINEAR,
    };

    enum TextureAddressMode : u32
    {
        LR_TEXTURE_ADDRESS_WRAP = 0,
        LR_TEXTURE_ADDRESS_MIRROR,
        LR_TEXTURE_ADDRESS_CLAMP_TO_EDGE,
        LR_TEXTURE_ADDRESS_CLAMP_TO_BORDER,
    };

}  // namespace lr