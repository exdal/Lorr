//
// Created on Monday 9th May 2022 by e-erdal
//

#pragma once

namespace lr::Graphics
{
    enum class APIFlags : u32
    {
        None,

        VSync = 1 << 0,
    };
    EnumFlags(APIFlags);

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

}  // namespace lr::Graphics