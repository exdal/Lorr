//
// Created on Tuesday 10th May 2022 by e-erdal
//

#pragma once

#include "VKSym.hh"

#include "Graphics/API/Common.hh"

namespace lr::g
{
    enum class ResourceFormat : u16
    {
        Unknown,
        BC1,

        RGBA8F,       /// Each channel is u8, packed into normalized u32
        RGBA8_SRGBF,  ///
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

    enum class ResourceType : u8
    {
        ColorAttachment,
        DepthAttachment,
        RenderTarget,
        ConstantBuffer,
        UnorderedAccess
    };

    struct ImageDesc
    {
        ResourceType Type = ResourceType::ColorAttachment;
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

    struct VKImage
    {
        VkImage m_pHandle = nullptr;
        VkImageView m_pViewHandle = nullptr;

        VkDeviceMemory m_pMemoryHandle = nullptr;

        u32 m_Width = 0;
        u32 m_Height = 0;
        u64 m_DataOffset = 0;
        u32 m_RequiredDataSize = 0;  // Required data size from Vulkan API
        u32 m_DataSize = 0;          // Real size of data

        u32 m_UsingMip = 0;
        u32 m_TotalMips = 1;

        ResourceType m_Type;
        ResourceFormat m_Format;
        bool m_Mappable = false;

        AllocatorType m_AllocatorType;
        void *m_pAllocatorData = nullptr;
    };

    struct BufferDesc
    {
        ResourceFormat Format = ResourceFormat::Unknown;
        bool Mappable = false;

        BufferUsage UsageFlags;
    };

    struct BufferData
    {
        u32 DataLen = 0;
        u8 *pData = nullptr;
    };

    struct VKBuffer
    {
        VkBuffer m_pHandle = nullptr;
        VkBufferView m_pViewHandle = nullptr;
        VkDeviceMemory m_pMemoryHandle = nullptr;

        u64 m_DataOffset = 0;
        u32 m_RequiredDataSize = 0;  // Required data size from Vulkan API
        u32 m_DataSize = 0;          // Real size of data

        ResourceType m_Type;
        ResourceFormat m_Format;
        bool m_Mappable = false;

        AllocatorType m_AllocatorType;
        void *m_pAllocatorData = nullptr;
    };

    /// -------------- Descriptors --------------- ///

    struct VKDescriptorBindingDesc
    {
        u32 BindingID = -1;
        DescriptorType Type;
        VkShaderStageFlagBits ShaderStageFlags = VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
        u32 ArraySize = 1;

        VKBuffer *pBuffer = nullptr;
        VKImage *pImage = nullptr;
    };

    struct VKDescriptorSetDesc
    {
        std::initializer_list<VKDescriptorBindingDesc> Bindings;
    };

    struct VKDescriptorSet
    {
        u32 BindingCount = 0;
        VKDescriptorBindingDesc pBindingInfos[8];
        VkDescriptorBufferInfo pBindingBufferInfos[8];

        VkDescriptorSet pHandle = nullptr;
        VkDescriptorSetLayout pSetLayoutHandle = nullptr;
    };

    /// ------------------------------------------

}  // namespace lr::g
