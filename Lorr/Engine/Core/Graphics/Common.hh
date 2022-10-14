//
// Created on Monday 9th May 2022 by e-erdal
//

#pragma once

namespace lr::Graphics
{
    enum class PrimitiveType
    {
        PointList,
        LineList,
        LineStrip,
        TriangleList,
        TriangleStrip,
        Patch
    };

    enum class DepthFunc : u8
    {
        Never = 1,
        Less,
        Equal,
        LessEqual,
        Greater,
        NotEqual,
        GreaterEqual,
        Always
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

    enum class ShaderType : u8
    {
        Vertex,
        Pixel,
        Compute,
        TessellationControl,
        TessellationEvaluation,

        Count
    };

    enum class DescriptorType : u8
    {
        Sampler,
        Texture,
        ConstantBuffer,
        StructuredBuffer,
        RWTexture,

        Count
    };

    enum class BufferUsage : i32
    {
        Vertex = 1 << 0,
        Index = 1 << 1,
        Unordered = 1 << 2,
        Indirect = 1 << 3,
        CopySrc = 1 << 4,
        CopyDst = 1 << 5,
        ConstantBuffer = 1 << 6,
    };

    EnumFlags(BufferUsage);

    enum class AllocatorType
    {
        None,

        Descriptor,    // A linear, small sized pool with CPUW flag for per-frame descriptor data
        BufferLinear,  // A linear, medium sized pool for buffers
        BufferTLSF,    // Large sized pool for large scene buffers
                       //
        ImageTLSF,         // Large sized pool for large images

        Count,
    };

}  // namespace lr::Graphics