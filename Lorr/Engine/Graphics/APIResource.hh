#pragma once

#include "APIAllocator.hh"
#include "Common.hh"

namespace lr::Graphics
{
/////////////////////////////////
// BUFFERS

struct BufferDesc
{
    BufferUsage m_UsageFlags = BufferUsage::Vertex;
    ResourceAllocator m_TargetAllocator = ResourceAllocator::None;

    u32 m_Stride = 1;
    u64 m_DataSize = 0;
};

struct Buffer : APIObject<VK_OBJECT_TYPE_BUFFER>
{
    ResourceAllocator m_Allocator = ResourceAllocator::None;
    u64 m_AllocatorData = ~0;

    u32 m_DescriptorIndex = -1;

    u32 m_Stride = 1;
    u64 m_DataSize = 0;
    u64 m_DataOffset = 0;
    u64 m_DeviceAddress = 0;

    VkBuffer m_pHandle = nullptr;
};

/////////////////////////////////
// IMAGES

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

/////////////////////////////////
// SAMPLERS

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

/////////////////////////////////
// DESCRIPTORS

struct DescriptorLayoutElement : VkDescriptorSetLayoutBinding
{
    DescriptorLayoutElement(
        u32 binding, DescriptorType type, ShaderStage stage, u32 arraySize = 1);
};

struct DescriptorSetLayout : APIObject<VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT>
{
    VkDescriptorSetLayout m_pHandle = VK_NULL_HANDLE;
};

struct DescriptorGetInfo
{
    DescriptorGetInfo() = default;
    DescriptorGetInfo(Buffer *pBuffer, DescriptorType type);
    DescriptorGetInfo(Image *pImage, DescriptorType type);
    DescriptorGetInfo(Sampler *pSampler);

    union
    {
        Buffer *m_pBuffer = nullptr;
        Image *m_pImage;
        Sampler *m_pSampler;
    };

    DescriptorType m_Type = DescriptorType::Sampler;
};

}  // namespace lr::Graphics