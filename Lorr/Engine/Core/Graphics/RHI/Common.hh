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

    enum class PrimitiveType
    {
        PointList,
        LineList,
        LineStrip,
        TriangleList,
        TriangleStrip,
        Patch
    };

    enum class CompareOp
    {
        Never,
        Less,
        Equal,
        LessEqual,
        Greater,
        NotEqual,
        GreaterEqual,
        Always
    };

    enum class StencilCompareOp
    {
        Keep,
        Zero,
        Replace,
        IncrAndClamp,
        DecrAndClamp,
        Invert,
        IncrAndWrap,
        DescAndWrap,
    };

    enum class CullMode : u8
    {
        None = 0,
        Front,
        Back,
    };

    enum class FillMode : u8
    {
        Fill = 0,
        Wireframe,
    };

    enum class BlendFactor
    {
        Zero = 0,
        One,

        SrcColor,
        InvSrcColor,

        SrcAlpha,
        InvSrcAlpha,
        DestAlpha,
        InvDestAlpha,

        DestColor,
        InvDestColor,

        SrcAlphaSat,
        ConstantColor,
        InvConstantColor,

        Src1Color,
        InvSrc1Color,
        Src1Alpha,
        InvSrc1Alpha,
    };

    enum class BlendOp
    {
        Add,
        Subtract,
        ReverseSubtract,
        Min,
        Max,
    };

    enum class Filtering
    {
        Nearest,
        Linear,
    };

    enum class TextureAddressMode
    {
        Wrap,
        Mirror,
        ClampToEdge,
        ClampToBorder,
    };

}  // namespace lr::Graphics