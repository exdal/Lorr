//
// Created on Wednesday 19th October 2022 by e-erdal
//

#pragma once

#include "Core/Graphics/RHI/APIConfig.hh"
#include "Core/Graphics/RHI/Common.hh"

#include "BaseShader.hh"

namespace lr::Graphics
{
    constexpr u32 kImageFormatSizeLUT[] = {
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

    constexpr u32 GetImageFormatSize(ImageFormat format)
    {
        return kImageFormatSizeLUT[(u32)format];
    }

    /// IMAGES ///

    struct ImageDesc
    {
        ResourceUsage m_UsageFlags = LR_RESOURCE_USAGE_UNKNOWN;
        ImageFormat m_Format = LR_IMAGE_FORMAT_UNKNOWN;
        APIAllocatorType m_TargetAllocator = LR_API_ALLOCATOR_NONE;

        u16 m_Width = 0;
        u16 m_Height = 0;
        u16 m_ArraySize = 1;
        u32 m_MipMapLevels = 1;

        u32 m_DataLen = 0;
        u32 m_DeviceDataLen = 0;
    };

    struct Image
    {
        ResourceUsage m_UsageFlags = LR_RESOURCE_USAGE_UNKNOWN;
        ImageFormat m_Format = LR_IMAGE_FORMAT_UNKNOWN;
        APIAllocatorType m_TargetAllocator = LR_API_ALLOCATOR_NONE;
        void *m_pAllocatorData = nullptr;

        u16 m_Width = 0;
        u16 m_Height = 0;
        u16 m_ArraySize = 1;
        u32 m_MipMapLevels = 1;

        u32 m_DataLen = 0;
        u32 m_DataOffset = 0;
        u32 m_DeviceDataLen = 0;
    };

    /// BUFFERS ///

    struct BufferDesc
    {
        ResourceUsage m_UsageFlags = LR_RESOURCE_USAGE_UNKNOWN;
        APIAllocatorType m_TargetAllocator = LR_API_ALLOCATOR_NONE;

        u32 m_Stride = 0;
        u32 m_DataLen = 0;
        u32 m_DeviceDataLen = 0;
    };

    struct Buffer
    {
        ResourceUsage m_UsageFlags = LR_RESOURCE_USAGE_UNKNOWN;
        APIAllocatorType m_TargetAllocator = LR_API_ALLOCATOR_NONE;
        void *m_pAllocatorData = nullptr;

        u32 m_Stride = 0;
        u32 m_DataLen = 0;
        u32 m_DataOffset = 0;
        u32 m_DeviceDataLen = 0;
    };

    /// SAMPLERS ///

    struct SamplerDesc
    {
        union
        {
            u32 _u_data = 0;
            struct
            {
                Filtering m_MinFilter : 1;
                Filtering m_MagFilter : 1;
                Filtering m_MipFilter : 1;
                TextureAddressMode m_AddressU : 2;
                TextureAddressMode m_AddressV : 2;
                TextureAddressMode m_AddressW : 2;
                u32 m_UseAnisotropy : 1;
                CompareOp m_CompareOp : 3;
            };
        };

        float m_MaxAnisotropy = 0;
        float m_MipLODBias = 0;
        float m_MinLOD = 0;
        float m_MaxLOD = 0;
    };

    struct Sampler
    {
    };

    /// DESCRIPTORS ///

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
        u8 m_BindingID = -1;
        DescriptorType m_Type;
        ShaderStage m_TargetShader;
        u32 m_ArraySize = 1;

        union
        {
            Sampler *m_pSampler = nullptr;
            Buffer *m_pBuffer;
            Image *m_pImage;
        };
    };

    struct DescriptorSetDesc
    {
        u32 m_BindingCount = 0;
        DescriptorBindingDesc m_pBindings[LR_MAX_DESCRIPTORS_PER_LAYOUT];
    };

    struct DescriptorSet
    {
        u32 m_BindingCount = 0;
        DescriptorType m_pBindingTypes[LR_MAX_DESCRIPTORS_PER_LAYOUT];
    };

}  // namespace lr::Graphics