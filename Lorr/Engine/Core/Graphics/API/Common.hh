//
// Created on Monday 9th May 2022 by e-erdal
//

#pragma once

#define HRCall(func, message)                                                                                      \
    if (FAILED(hr = func))                                                                                         \
    {                                                                                                              \
        char *pError;                                                                                              \
        FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, \
                      NULL,                                                                                        \
                      hr,                                                                                          \
                      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),                                                   \
                      (LPTSTR)&pError,                                                                             \
                      0,                                                                                           \
                      NULL);                                                                                       \
        LOG_ERROR(message " HR: {}", pError);                                                                      \
        return;                                                                                                    \
    }

#define HRCallRet(func, message, ret)                                                                              \
    if (FAILED(hr = func))                                                                                         \
    {                                                                                                              \
        char *pError;                                                                                              \
        FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, \
                      NULL,                                                                                        \
                      hr,                                                                                          \
                      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),                                                   \
                      (LPTSTR)&pError,                                                                             \
                      0,                                                                                           \
                      NULL);                                                                                       \
        LOG_ERROR(message " HR: {}", pError);                                                                      \
        return ret;                                                                                                \
    }

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

    enum class BufferUsage : u32
    {
        Vertex = 1 << 0,
        Index = 1 << 1,
        Unordered = 1 << 2,
        Indirect = 1 << 3,
        CopySrc = 1 << 4,
        CopyDst = 1 << 5,
        ConstantBuffer = 1 << 6
    };

    EnumFlags(BufferUsage);

    enum class AllocatorType
    {
        None,
        Linear,
        TLSF
    };

}  // namespace lr::Graphics