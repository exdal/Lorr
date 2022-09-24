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
        D24FS8U,      /// Z-Buffer format, 24 bits for depth, 8 bits for stencil

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
            case ResourceFormat::R32F: return sizeof(float);
            case ResourceFormat::D32F: return sizeof(float);
            case ResourceFormat::D24FS8U: return sizeof(u32);
            default: return 0;
        }
    }

    enum class ResourceType : u8
    {
        ShaderResource,
        Depth,
        RenderTarget,
        ConstantBuffer,
        UnorderedAccess
    };

    struct ImageDesc
    {
        ResourceFormat Format = ResourceFormat::Unknown;

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
        void Init(ImageDesc *pDesc, ImageData *pData);

        void Delete();
        ~VKImage();

        VkImage m_pHandle = nullptr;
        VkImageView m_pViewHandle = nullptr;

        u32 m_Width = 0;
        u32 m_Height = 0;
        u32 m_DataSize = 0;

        u32 m_UsingMip = 0;
        u32 m_TotalMips = 1;

        ResourceType m_Type;
        ResourceFormat m_Format;
    };

    struct BufferDesc
    {
        ResourceFormat Format = ResourceFormat::Unknown;
    };

    struct BufferData
    {
        u32 DataLen = 0;
        u8 *pData = nullptr;
    };

    struct VKBuffer
    {
        void Init(BufferDesc *pDesc, VKBuffer *pData);

        void Delete();
        ~VKBuffer();

        VkBuffer m_pHandle = nullptr;
        VkBufferView m_pViewHandle = nullptr;
        VkDeviceMemory m_pMemoryHandle = nullptr;

        u32 m_DataSize = 0;

        ResourceType m_Type;
        ResourceFormat m_Format;
    };

    /// -------------- Descriptors ---------------

    struct VKDescriptorBindingDesc
    {
        u32 BindingID = -1;
        DescriptorType Type;
        VkShaderStageFlagBits ShaderStageFlags = VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
        u32 ArraySize = 1;
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
