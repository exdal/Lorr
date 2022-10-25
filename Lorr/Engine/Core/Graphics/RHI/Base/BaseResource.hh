//
// Created on Wednesday 19th October 2022 by e-erdal
//

#pragma once

#include "Core/Graphics/RHI/Common.hh"

namespace lr::Graphics
{
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

    enum class ImageUsage : i32
    {
        ColorAttachment = 1 << 0,
        DepthAttachment = 1 << 1,
        Sampled = 1 << 2,
        CopySrc = 1 << 3,
        CopyDst = 1 << 4,
    };

    EnumFlags(ImageUsage);

    enum class AllocatorType
    {
        None,

        Descriptor,    // A linear, small sized pool with CPUW flag for per-frame descriptor data
        BufferLinear,  // A linear, medium sized pool for buffers
        BufferTLSF,    // Large sized pool for large scene buffers
                       //
        ImageTLSF,     // Large sized pool for large images

        Count,
    };

    enum class ResourceFormat : u16
    {
        Unknown,
        BC1,

        RGBA8F,       /// Each channel is u8, packed into normalized u32
        RGBA8_SRGBF,  ///
        BGRA8F,       ///
        RGBA16F,      ///
        RGBA32F,      /// Each channel is float
        R32U,         /// R channel is 32 bits u32
        R32F,         /// R channel is 32 bits float
        D32F,         /// Depth format, A channel is float
        D32FS8U,      /// Depth format, 32 bits for depth, 8 bits for stencil

    };

    constexpr u32 GetResourceFormatSize(ResourceFormat format)
    {
        switch (format)
        {
            case ResourceFormat::BC1: return sizeof(u8);
            case ResourceFormat::RGBA8_SRGBF:
            case ResourceFormat::RGBA8F: return sizeof(u8) * 4;
            case ResourceFormat::RGBA16F: return sizeof(u16) * 4;
            case ResourceFormat::RGBA32F: return sizeof(float) * 4;
            case ResourceFormat::R32U: return sizeof(u32);
            case ResourceFormat::R32F: return sizeof(f32);
            case ResourceFormat::D32F: return sizeof(f32);
            case ResourceFormat::D32FS8U: return sizeof(u32);
            default: return 0;
        }
    }

    /// IMAGES ///

    struct ImageDesc
    {
        ImageUsage Usage;
        ResourceFormat Format = ResourceFormat::Unknown;
        bool Mappable = false;

        u32 Alignment = 0;
        u16 ArraySize = 1;
        u32 MipMapLevels = 1;
    };

    struct ImageData
    {
        u32 Width = 0;
        u32 Height = 0;

        u32 DataLen = 0;
        u8 *pData = nullptr;
    };

    struct BaseImage
    {
        u32 m_Width = 0;
        u32 m_Height = 0;
        u64 m_DataOffset = 0;
        u32 m_DataSize = 0;

        u32 m_UsingMip = 0;
        u32 m_TotalMips = 1;

        ImageUsage m_Usage;
        ResourceFormat m_Format;

        AllocatorType m_AllocatorType;
        void *m_pAllocatorData = nullptr;
    };

    /// BUFFERS ///

    struct BufferDesc
    {
        bool Mappable = false;

        BufferUsage UsageFlags;
    };

    struct BufferData
    {
        u32 DataLen = 0;
        u8 *pData = nullptr;
    };

    struct BaseBuffer
    {
        u64 m_DataOffset = 0;
        u32 m_RequiredDataSize = 0;  // Required data size from Vulkan API
        u32 m_DataSize = 0;          // Real size of data

        BufferUsage m_Usage;
        ResourceFormat m_Format;
        bool m_Mappable = false;

        AllocatorType m_AllocatorType;
        void *m_pAllocatorData = nullptr;
    };

}  // namespace lr::Graphics