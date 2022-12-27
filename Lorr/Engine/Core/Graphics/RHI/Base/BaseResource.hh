//
// Created on Wednesday 19th October 2022 by e-erdal
//

#pragma once

#include "Core/Graphics/RHI/Common.hh"

#include "BaseShader.hh"

namespace lr::Graphics
{
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

        LR_RESOURCE_USAGE_PRESENT = LR_RESOURCE_USAGE_SHADER_RESOURCE | LR_RESOURCE_USAGE_RENDER_TARGET,

        LR_RESOURCE_USAGE_MAX = 1U << 31,
    };
    EnumFlags(ResourceUsage);

    enum class AllocatorType
    {
        None,

        Descriptor,              // A linear, small sized pool with CPUW flag for per-frame descriptor data
        BufferLinear,            // A linear, medium sized pool for buffers
        BufferTLSF,              // Large sized pool for large scene buffers
        BufferFrameTime = None,  //
                                 //
        ImageTLSF,               // Large sized pool for large images

        Count,
    };

    enum ResourceFormat : u32
    {
        LR_RESOURCE_FORMAT_UNKNOWN,
        LR_RESOURCE_FORMAT_RGBA8F,
        LR_RESOURCE_FORMAT_RGBA8_SRGBF,
        LR_RESOURCE_FORMAT_BGRA8F,
        LR_RESOURCE_FORMAT_RGBA16F,
        LR_RESOURCE_FORMAT_RGBA32F,
        LR_RESOURCE_FORMAT_R32U,
        LR_RESOURCE_FORMAT_R32F,
        LR_RESOURCE_FORMAT_D32F,
        LR_RESOURCE_FORMAT_D32FS8U,
    };

    constexpr u32 kResourceFormatSizeLUT[] = {
        0,                // LR_RESOURCE_FORMAT_UNKNOWN
        sizeof(u8) * 4,   // LR_RESOURCE_FORMAT_RGBA8F
        sizeof(u8) * 4,   // LR_RESOURCE_FORMAT_RGBA8_SRGBF
        sizeof(u8) * 4,   // LR_RESOURCE_FORMAT_BGRA8F
        sizeof(u16) * 4,  // LR_RESOURCE_FORMAT_RGBA16F
        sizeof(u32) * 4,  // LR_RESOURCE_FORMAT_RGBA32F
        sizeof(u32),      // LR_RESOURCE_FORMAT_R32U
        sizeof(u32),      // LR_RESOURCE_FORMAT_R32F
        sizeof(u32),      // LR_RESOURCE_FORMAT_D32F
        sizeof(u32),      // LR_RESOURCE_FORMAT_D32FS8U
    };

    constexpr u32 GetResourceFormatSize(ResourceFormat format)
    {
        return kResourceFormatSizeLUT[(u32)format];
    }

    /// IMAGES ///

    struct ImageDesc
    {
        ResourceUsage UsageFlags = LR_RESOURCE_USAGE_UNKNOWN;
        ResourceFormat Format = LR_RESOURCE_FORMAT_UNKNOWN;

        u16 ArraySize = 1;
        u32 MipMapLevels = 1;
    };

    struct ImageData
    {
        AllocatorType TargetAllocator = AllocatorType::None;

        u32 Width = 0;
        u32 Height = 0;

        u32 DataLen = 0;
        u8 *pData = nullptr;
    };

    struct BaseImage
    {
        ResourceUsage m_UsageFlags = LR_RESOURCE_USAGE_UNKNOWN;
        ResourceFormat m_Format;

        u32 m_Width = 0;
        u32 m_Height = 0;
        u64 m_DataOffset = 0;
        u32 m_DataSize = 0;
        u32 m_RequiredDataSize = 0;  // Required data size from API

        u32 m_UsingMip = 0;
        u32 m_TotalMips = 1;

        AllocatorType m_AllocatorType;
        void *m_pAllocatorData = nullptr;
    };

    /// BUFFERS ///

    struct BufferDesc
    {
        ResourceUsage UsageFlags = LR_RESOURCE_USAGE_UNKNOWN;
    };

    struct BufferData
    {
        AllocatorType TargetAllocator = AllocatorType::None;

        bool Mappable = false;

        u32 Stride = 0;
        u32 DataLen = 0;
        u8 *pData = nullptr;
    };

    struct BaseBuffer
    {
        u64 m_DataOffset = 0;
        u32 m_RequiredDataSize = 0;  // Required data size from Vulkan API
        u32 m_DataSize = 0;          // Real size of data
        u32 m_Stride = 0;

        ResourceUsage m_UsageFlags = LR_RESOURCE_USAGE_UNKNOWN;
        ResourceFormat m_Format;
        bool m_Mappable = false;

        AllocatorType m_AllocatorType;
        void *m_pAllocatorData = nullptr;
    };

    /// SAMPLERS ///

    struct SamplerDesc
    {
        union
        {
            u32 _u_data = 0;
            struct
            {
                Filtering MinFilter : 1;
                Filtering MagFilter : 1;
                Filtering MipFilter : 1;
                TextureAddressMode AddressU : 2;
                TextureAddressMode AddressV : 2;
                TextureAddressMode AddressW : 2;
                u32 UseAnisotropy : 1;
                CompareOp CompareOp : 3;
            };
        };

        float MaxAnisotropy = 0;
        float MipLODBias = 0;
        float MinLOD = 0;
        float MaxLOD = 0;
    };

    enum DescriptorType : u8
    {
        LR_DESCRIPTOR_TYPE_SAMPLER = 0,
        LR_DESCRIPTOR_TYPE_SHADER_RESOURCE,
        LR_DESCRIPTOR_TYPE_CONSTANT_BUFFER,
        LR_DESCRIPTOR_TYPE_UNORDERED_ACCESS_IMAGE,
        LR_DESCRIPTOR_TYPE_UNORDERED_ACCESS_BUFFER,
        LR_DESCRIPTOR_TYPE_PUSH_CONSTANT,

        LR_DESCRIPTOR_TYPE_COUNT,
    };

    struct DescriptorBindingDesc
    {
        u8 BindingID = -1;
        DescriptorType Type;
        ShaderStage TargetShader;
        u32 ArraySize = 1;

        union
        {
            SamplerDesc *pSamplerInfo = nullptr;
            BaseBuffer *pBuffer;
            BaseImage *pImage;
        };
    };

    struct DescriptorSetDesc
    {
        u32 BindingCount = 0;
        DescriptorBindingDesc pBindings[8];
    };

    struct BaseDescriptorSet
    {
        u32 BindingCount = 0;
        DescriptorBindingDesc pBindings[8];
    };

}  // namespace lr::Graphics