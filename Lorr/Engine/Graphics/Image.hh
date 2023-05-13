//
// Created on Saturday 22nd April 2023 by exdal
//

#pragma once

#include "APIAllocator.hh"

namespace lr::Graphics
{
enum class ImageUsage : u32
{
    Sampled = 1 << 1,  // Read only image
    ColorAttachment = 1 << 2,
    DepthStencilAttachment = 1 << 3,
    TransferSrc = 1 << 4,
    TransferDst = 1 << 5,
    Storage = 1 << 6,     // RWA(Read-Write-Atomic) image
    Present = 1 << 7,     // Virtual flag
    Concurrent = 1 << 8,  // Vulkan's sharing flag

    AspectColor = Sampled | ColorAttachment | Storage,
    AspectDepthStencil = DepthStencilAttachment,
};
EnumFlags(ImageUsage);

enum class ImageFormat : u32
{
    Unknown = 0,
    RGBA8F,
    RGBA8_SRGBF,
    BGRA8F,
    RGBA16F,
    RGBA32F,
    R32U,
    R32F,
    D32F,
    D32FS8U,
};

enum class ImageLayout : u32
{
    Undefined = 0,
    Present,
    ColorAttachment,
    DepthStencilAttachment,
    ColorReadOnly,
    DepthStencilReadOnly,
    TransferSrc,
    TransferDst,
    General,
};

struct ImageDesc
{
    ImageUsage m_UsageFlags = ImageUsage::Sampled;
    ImageFormat m_Format = ImageFormat::Unknown;
    ResourceAllocator m_TargetAllocator = ResourceAllocator::None;

    u32 m_Width = 0;
    u32 m_Height = 0;
    u16 m_ArraySize = 1;
    u32 m_MipMapLevels = 1;

    u64 m_DataLen = 0;
};

struct Image : APIObject<VK_OBJECT_TYPE_IMAGE>
{
    void Init(ImageDesc *pDesc)
    {
        m_Format = pDesc->m_Format;
        m_TargetAllocator = pDesc->m_TargetAllocator;
        m_Width = pDesc->m_Width;
        m_Height = pDesc->m_Height;
        m_ArraySize = pDesc->m_ArraySize;
        m_MipMapLevels = pDesc->m_MipMapLevels;
        m_DataLen = pDesc->m_DataLen;
    }

    ImageFormat m_Format = ImageFormat::Unknown;
    ResourceAllocator m_TargetAllocator = ResourceAllocator::None;
    void *m_pAllocatorData = nullptr;

    u32 m_Width = 0;
    u32 m_Height = 0;
    u32 m_ArraySize = 1;
    u32 m_MipMapLevels = 1;
    VkImageAspectFlags m_ImageAspect = 0;
    u32 m_DescriptorIndex = -1;

    u64 m_DataLen = 0;
    u64 m_AllocatorOffset = 0;
    u64 m_DeviceDataLen = 0;

    VkImage m_pHandle = nullptr;
    VkImageView m_pViewHandle = nullptr;
    VkDeviceMemory m_pMemoryHandle = nullptr;
};

constexpr u32 kImageFormatSizeLUT[] = {
    0,                // UNKNOWN
    sizeof(u8) * 4,   // RGBA8F
    sizeof(u8) * 4,   // RGBA8_SRGBF
    sizeof(u8) * 4,   // BGRA8F
    sizeof(u16) * 4,  // RGBA16F
    sizeof(u32) * 4,  // RGBA32F
    sizeof(u32),      // R32U
    sizeof(u32),      // R32F
    sizeof(u32),      // D32F
    sizeof(u32),      // D32FS8U
};

constexpr u32 GetImageFormatSize(ImageFormat format)
{
    return kImageFormatSizeLUT[(u32)format];
}

enum class Filtering : u32
{
    Nearest = 0,
    Linear,
};

enum class TextureAddressMode : u32
{
    Wrap = 0,
    Mirror,
    ClampToEdge,
    ClampToBorder,
};

enum class CompareOp : u32
{
    Never = 0,
    Less,
    Equal,
    LessEqual,
    Greater,
    NotEqual,
    GreaterEqual,
    Always,
};

struct SamplerDesc
{
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

    float m_MaxAnisotropy = 0;
    float m_MipLODBias = 0;
    float m_MinLOD = 0;
    float m_MaxLOD = 0;
};

struct Sampler : APIObject<VK_OBJECT_TYPE_SAMPLER>
{
    VkSampler m_pHandle = nullptr;
    u32 m_DescriptorIndex = -1;
};

}  // namespace lr::Graphics