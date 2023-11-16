#pragma once

#include "APIObject.hh"
#include "Common.hh"

namespace lr::Graphics
{
/////////////////////////////////
// BUFFERS

struct BufferDesc
{
    BufferUsage m_UsageFlags = BufferUsage::Vertex;

    u32 m_Stride = 1;
    u64 m_DataSize = 0;
};

struct Buffer : APIObject
{
    u64 m_AllocatorData = ~0;

    u32 m_Stride = 1;
    u64 m_DataSize = 0;
    u64 m_DataOffset = 0;
    u64 m_DeviceAddress = 0;

    VkBuffer m_pHandle = nullptr;

    operator VkBuffer &() { return m_pHandle; }
};
LR_ASSIGN_OBJECT_TYPE(Buffer, VK_OBJECT_TYPE_BUFFER);

/////////////////////////////////
// IMAGES

struct ImageDesc
{
    ImageUsage m_UsageFlags = ImageUsage::Sampled;
    Format m_Format = Format::Unknown;

    u32 m_Width = 0;
    u32 m_Height = 0;
    u32 m_ArraySize = 1;
    u32 m_MipMapLevels = 1;
    eastl::span<u32> m_QueueIndices = {};

    u64 m_DataSize = 0;
};

struct Image : APIObject
{
    Format m_Format = Format::Unknown;
    u64 m_AllocatorData = ~0;

    u32 m_Width = 0;
    u32 m_Height = 0;
    u32 m_ArraySize = 1;
    u32 m_MipMapLevels = 1;

    u64 m_DataSize = 0;    // Aligned data offset
    u64 m_DataOffset = 0;  // Allocator offset

    VkImage m_pHandle = nullptr;

    operator VkImage &() { return m_pHandle; }
};
LR_ASSIGN_OBJECT_TYPE(Image, VK_OBJECT_TYPE_IMAGE);

// Image views can have different formats depending on the creation flags
// Currently, we do not support it so view's format is automatically parent
// image format. See:
// https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkImageViewCreateInfo.html

struct ImageSubresourceInfo
{
    ImageAspect m_AspectMask = ImageAspect::Color;
    u32 m_BaseMip : 8 = 0;
    u32 m_MipCount : 8 = 1;
    u32 m_BaseSlice : 8 = 0;
    u32 m_SliceCount : 8 = 1;
};

struct ImageViewDesc
{
    Image *m_pImage = nullptr;
    ImageViewType m_Type = ImageViewType::View2D;
    // Component mapping
    ImageComponentSwizzle m_SwizzleR = ImageComponentSwizzle::Identity;
    ImageComponentSwizzle m_SwizzleG = ImageComponentSwizzle::Identity;
    ImageComponentSwizzle m_SwizzleB = ImageComponentSwizzle::Identity;
    ImageComponentSwizzle m_SwizzleA = ImageComponentSwizzle::Identity;
    ImageSubresourceInfo m_SubresourceInfo = {};
};

struct ImageSubresourceRange : VkImageSubresourceRange
{
    ImageSubresourceRange(ImageSubresourceInfo info);
};

struct ImageView : APIObject
{
    Format m_Format = Format::Unknown;
    ImageViewType m_Type = ImageViewType::View2D;
    ImageSubresourceInfo m_SubresourceInfo = {};

    VkImageView m_pHandle = nullptr;

    operator VkImageView &() { return m_pHandle; }
};
LR_ASSIGN_OBJECT_TYPE(ImageView, VK_OBJECT_TYPE_IMAGE_VIEW);

/////////////////////////////////
// SAMPLERS

struct SamplerDesc
{
    Filtering m_MinFilter : 1;
    Filtering m_MagFilter : 1;
    Filtering m_MipFilter : 1;
    TextureAddressMode m_AddressU : 2;
    TextureAddressMode m_AddressV : 2;
    TextureAddressMode m_AddressW : 2;
    u32 m_UseAnisotropy : 1;
    CompareOp m_CompareOp : 3;
    float m_MaxAnisotropy = 0;
    float m_MipLODBias = 0;
    float m_MinLOD = 0;
    float m_MaxLOD = 0;
};

struct Sampler : APIObject
{
    VkSampler m_pHandle = nullptr;

    operator VkSampler &() { return m_pHandle; }
};
LR_ASSIGN_OBJECT_TYPE(Sampler, VK_OBJECT_TYPE_SAMPLER);

/////////////////////////////////
// DESCRIPTORS

struct DescriptorLayoutElement : VkDescriptorSetLayoutBinding
{
    DescriptorLayoutElement(
        u32 binding, DescriptorType type, ShaderStage stage, u32 arraySize = 1);
};

using DescriptorSetLayout = VkDescriptorSetLayout;
LR_ASSIGN_OBJECT_TYPE(DescriptorSetLayout, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT);

struct DescriptorGetInfo
{
    DescriptorGetInfo() = default;
    DescriptorGetInfo(Buffer *pBuffer, u64 dataSize, DescriptorType type);
    DescriptorGetInfo(ImageView *pImageView, DescriptorType type);
    DescriptorGetInfo(Sampler *pSampler);

    union
    {
        struct
        {
            u64 m_BufferAddress = 0;
            u64 m_DataSize = 0;
        };
        ImageView *m_pImageView;
        Sampler *m_pSampler;
        bool m_HasDescriptor;
    };

    DescriptorType m_Type = DescriptorType::Sampler;
};

}  // namespace lr::Graphics