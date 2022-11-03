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

    enum class DepthCompareOp
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

    enum class TextureFiltering : u8
    {
        Point = 0,
        Linear,
        Ansio,

        Count
    };

    enum class TextureAddress : u8
    {
        Wrap,
        Mirror,
        Clamp,
        Border,
    };

    enum class CommandListType : u8
    {
        Direct,
        Compute,
        Copy,
        // TODO: Video types
    };

    enum class DescriptorType : u8
    {
        Sampler,
        ShaderResourceView,
        ConstantBufferView,
        UnorderedAccessBuffer,
        UnorderedAccessView,
        RootConstant,

        Count,
    };

    enum class ShaderStage
    {
        None = 0,
        Vertex = 1 << 0,
        Pixel = 1 << 1,
        Compute = 1 << 2,
        Hull = 1 << 3,
        Domain = 1 << 4,
        Geometry = 1 << 5,
    };

    EnumFlags(ShaderStage);

}  // namespace lr::Graphics