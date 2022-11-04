//
// Created on Wednesday 19th October 2022 by e-erdal
//

#pragma once

#include "Core/Graphics/RHI/Common.hh"

namespace lr::Graphics
{
    enum class ResourceUsage : u32
    {
        Undefined = 0,

        VertexBuffer = 1 << 0,
        IndexBuffer = 1 << 1,

        IndirectBuffer = 1 << 2,

        ConstantBuffer = 1 << 3,
        ShaderResource = 1 << 4,
        RenderTarget = 1 << 5,
        DepthStencilWrite = 1 << 6,
        DepthStencilRead = 1 << 7,

        CopySrc = 1 << 8,
        CopyDst = 1 << 9,

        Present = 1 << 10,

        UnorderedAccess = 1 << 11,
    };

    EnumFlags(ResourceUsage);

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
        ResourceUsage Usage;
        ResourceFormat Format = ResourceFormat::Unknown;
        bool Mappable = false;

        AllocatorType TargetAllocator = AllocatorType::None;

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
        ResourceUsage m_Usage;
        ResourceFormat m_Format;

        u32 m_Width = 0;
        u32 m_Height = 0;
        u64 m_DataOffset = 0;
        u32 m_DataSize = 0;

        u32 m_UsingMip = 0;
        u32 m_TotalMips = 1;

        AllocatorType m_AllocatorType;
        void *m_pAllocatorData = nullptr;
    };

    /// BUFFERS ///

    struct BufferDesc
    {
        ResourceUsage UsageFlags;
        bool Mappable = false;

        AllocatorType TargetAllocator = AllocatorType::None;
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

        ResourceUsage m_Usage;
        ResourceFormat m_Format;
        bool m_Mappable = false;

        AllocatorType m_AllocatorType;
        void *m_pAllocatorData = nullptr;
    };

    struct DescriptorBindingDesc
    {
        // u32 BindingID = -1;
        DescriptorType Type;
        ShaderStage TargetShader;
        u32 ArraySize = 1;

        BaseBuffer *pBuffer = nullptr;
        BaseImage *pImage = nullptr;
    };

    struct DescriptorSetDesc
    {
        u32 BindingCount = 0;
        DescriptorBindingDesc pBindings[8];
    };

    struct BaseDescriptorSet
    {
        u32 BindingCount = 0;
        DescriptorBindingDesc pBindingInfos[8];
    };

}  // namespace lr::Graphics