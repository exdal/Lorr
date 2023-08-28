// Created on Saturday April 22nd 2023 by exdal
// Last modified on Monday August 28th 2023 by exdal

#pragma once

#include "APIAllocator.hh"
#include "Common.hh"

namespace lr::Graphics
{
struct ImageDesc
{
    ImageUsage m_UsageFlags = ImageUsage::Sampled;
    Format m_Format = Format::Unknown;
    ResourceAllocator m_TargetAllocator = ResourceAllocator::None;

    u32 m_Width = 0;
    u32 m_Height = 0;
    u32 m_ArraySize = 1;
    u32 m_MipMapLevels = 1;

    u64 m_DataSize = 0;
};

struct Image : APIObject<VK_OBJECT_TYPE_IMAGE>
{
    Format m_Format = Format::Unknown;
    ResourceAllocator m_Allocator = ResourceAllocator::None;
    u64 m_AllocatorData = ~0;

    u32 m_Width = 0;
    u32 m_Height = 0;
    u32 m_ArraySize = 1;
    u32 m_MipMapLevels = 1;
    u32 m_DescriptorIndex = -1;

    u64 m_DataSize = 0;    // Aligned data offset
    u64 m_DataOffset = 0;  // Allocator offset

    VkImage m_pHandle = nullptr;
    VkImageView m_pViewHandle = nullptr;
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