//
// Created on Tuesday 10th May 2022 by exdal
//

#pragma once

#include "Allocator.hh"
#include "Config.hh"

#include "VKShader.hh"

#include "Vulkan.hh"

namespace lr::Graphics
{
enum APIAllocatorType : u32
{
    LR_API_ALLOCATOR_NONE = 0,

    LR_API_ALLOCATOR_DESCRIPTOR,     // LINEAR - small sized pool with CPUW flag for per-frame descriptor data
    LR_API_ALLOCATOR_BUFFER_LINEAR,  // LINEAR - medium sized pool for buffers
    LR_API_ALLOCATOR_BUFFER_TLSF,    // TLSF - Large sized pool for large scene buffers
    LR_API_ALLOCATOR_BUFFER_TLSF_HOST,  // TLSF - medium sized pool for host visible buffers like UI
    LR_API_ALLOCATOR_BUFFER_FRAMETIME,  // LINEAR - Small pool for frametime buffers
    LR_API_ALLOCATOR_IMAGE_TLSF,        // TLSF - Large sized pool for large images

    LR_API_ALLOCATOR_COUNT,  // Special flag, basically telling API to not allocate anything (ie. SC images)
};

/// IMAGES ///

enum ImageUsage : u32
{
    LR_IMAGE_USAGE_SHADER_RESOURCE = 1 << 1,
    LR_IMAGE_USAGE_RENDER_TARGET = 1 << 2,
    LR_IMAGE_USAGE_DEPTH_STENCIL = 1 << 3,
    LR_IMAGE_USAGE_TRANSFER_SRC = 1 << 4,
    LR_IMAGE_USAGE_TRANSFER_DST = 1 << 5,
    LR_IMAGE_USAGE_UNORDERED_ACCESS = 1 << 6,
    LR_IMAGE_USAGE_PRESENT = 1 << 7,
};
EnumFlags(ImageUsage);

enum ImageFormat : u32
{
    LR_IMAGE_FORMAT_UNKNOWN = 0,
    LR_IMAGE_FORMAT_RGBA8F,
    LR_IMAGE_FORMAT_RGBA8_SRGBF,
    LR_IMAGE_FORMAT_BGRA8F,
    LR_IMAGE_FORMAT_RGBA16F,
    LR_IMAGE_FORMAT_RGBA32F,
    LR_IMAGE_FORMAT_R32U,
    LR_IMAGE_FORMAT_R32F,
    LR_IMAGE_FORMAT_D32F,
    LR_IMAGE_FORMAT_D32FS8U,
};

enum ImageLayout : u32
{
    LR_IMAGE_LAYOUT_UNDEFINED = 0,
    LR_IMAGE_LAYOUT_PRESENT,
    LR_IMAGE_LAYOUT_ATTACHMENT,
    LR_IMAGE_LAYOUT_READ_ONLY,
    LR_IMAGE_LAYOUT_TRANSFER_SRC,
    LR_IMAGE_LAYOUT_TRANSFER_DST,
    LR_IMAGE_LAYOUT_UNORDERED_ACCESS,
};

struct ImageDesc
{
    ImageUsage m_UsageFlags = LR_IMAGE_USAGE_SHADER_RESOURCE;
    ImageFormat m_Format = LR_IMAGE_FORMAT_UNKNOWN;
    APIAllocatorType m_TargetAllocator = LR_API_ALLOCATOR_NONE;

    u32 m_Width = 0;
    u32 m_Height = 0;
    u16 m_ArraySize = 1;
    u32 m_MipMapLevels = 1;

    u32 m_DataLen = 0;
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

    ImageFormat m_Format = LR_IMAGE_FORMAT_UNKNOWN;
    ImageLayout m_Layout = LR_IMAGE_LAYOUT_UNDEFINED;
    APIAllocatorType m_TargetAllocator = LR_API_ALLOCATOR_NONE;
    void *m_pAllocatorData = nullptr;

    u32 m_Width = 0;
    u32 m_Height = 0;
    u16 m_ArraySize = 1;
    u32 m_MipMapLevels = 1;
    VkImageAspectFlags m_ImageAspect = 0;

    u32 m_DataLen = 0;
    u32 m_DeviceDataOffset = 0;
    u32 m_DeviceDataLen = 0;

    VkImage m_pHandle = nullptr;
    VkImageView m_pViewHandle = nullptr;
    VkDeviceMemory m_pMemoryHandle = nullptr;
};

constexpr u32 kImageFormatSizeLUT[] = {
    0,                // LR_IMAGE_FORMAT_UNKNOWN
    sizeof(u8) * 4,   // LR_IMAGE_FORMAT_RGBA8F
    sizeof(u8) * 4,   // LR_IMAGE_FORMAT_RGBA8_SRGBF
    sizeof(u8) * 4,   // LR_IMAGE_FORMAT_BGRA8F
    sizeof(u16) * 4,  // LR_IMAGE_FORMAT_RGBA16F
    sizeof(u32) * 4,  // LR_IMAGE_FORMAT_RGBA32F
    sizeof(u32),      // LR_IMAGE_FORMAT_R32U
    sizeof(u32),      // LR_IMAGE_FORMAT_R32F
    sizeof(u32),      // LR_IMAGE_FORMAT_D32F
    sizeof(u32),      // LR_IMAGE_FORMAT_D32FS8U
};

constexpr u32 GetImageFormatSize(ImageFormat format)
{
    return kImageFormatSizeLUT[(u32)format];
}

/// BUFFERS ///

enum BufferUsage : u32
{
    LR_BUFFER_USAGE_VERTEX_BUFFER = 1 << 0,
    LR_BUFFER_USAGE_INDEX_BUFFER = 1 << 1,
    LR_BUFFER_USAGE_CONSTANT_BUFFER = 1 << 2,
    LR_BUFFER_USAGE_TRANSFER_SRC = 1 << 3,
    LR_BUFFER_USAGE_TRANSFER_DST = 1 << 4,
    LR_BUFFER_USAGE_UNORDERED_ACCESS = 1 << 5,
};
EnumFlags(BufferUsage);

struct BufferDesc
{
    BufferUsage m_UsageFlags = LR_BUFFER_USAGE_VERTEX_BUFFER;
    APIAllocatorType m_TargetAllocator = LR_API_ALLOCATOR_NONE;

    u32 m_Stride = 0;
    u32 m_DataLen = 0;
};

struct Buffer : APIObject<VK_OBJECT_TYPE_BUFFER>
{
    void Init(BufferDesc *pDesc)
    {
        m_UsageFlags = pDesc->m_UsageFlags;
        m_TargetAllocator = pDesc->m_TargetAllocator;
        m_Stride = pDesc->m_Stride;
        m_DataLen = pDesc->m_DataLen;
    }

    BufferUsage m_UsageFlags = LR_BUFFER_USAGE_VERTEX_BUFFER;
    APIAllocatorType m_TargetAllocator = LR_API_ALLOCATOR_NONE;
    void *m_pAllocatorData = nullptr;

    u32 m_Stride = 1;
    u32 m_DataLen = 0;

    u32 m_DeviceDataOffset = 0;
    u32 m_DeviceDataLen = 0;

    VkBuffer m_pHandle = nullptr;
    VkBufferView m_pViewHandle = nullptr;
    VkDeviceMemory m_pMemoryHandle = nullptr;
};

/// SAMPLERS ///

enum Filtering : u32
{
    LR_FILTERING_NEAREST = 0,
    LR_FILTERING_LINEAR,
};

enum TextureAddressMode : u32
{
    LR_TEXTURE_ADDRESS_WRAP = 0,
    LR_TEXTURE_ADDRESS_MIRROR,
    LR_TEXTURE_ADDRESS_CLAMP_TO_EDGE,
    LR_TEXTURE_ADDRESS_CLAMP_TO_BORDER,
};

enum CompareOp : u32
{
    LR_COMPARE_OP_NEVER = 0,
    LR_COMPARE_OP_LESS,
    LR_COMPARE_OP_EQUAL,
    LR_COMPARE_OP_LESS_EQUAL,
    LR_COMPARE_OP_GREATER,
    LR_COMPARE_OP_NOT_EQUAL,
    LR_COMPARE_OP_GREATER_EQUAL,
    LR_COMPARE_OP_ALWAYS,
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
};

/// DESCRIPTORS ///

enum DescriptorType : u32
{
    LR_DESCRIPTOR_TYPE_SAMPLER = 0,
    LR_DESCRIPTOR_TYPE_SHADER_RESOURCE_IMAGE,
    LR_DESCRIPTOR_TYPE_SHADER_RESOURCE_BUFFER,  // Read-only buffer
    LR_DESCRIPTOR_TYPE_CONSTANT_BUFFER,
    LR_DESCRIPTOR_TYPE_UNORDERED_ACCESS_IMAGE,
    LR_DESCRIPTOR_TYPE_UNORDERED_ACCESS_BUFFER,  // Write-read buffer
    LR_DESCRIPTOR_TYPE_PUSH_CONSTANT,

    LR_DESCRIPTOR_TYPE_COUNT,
};

struct DescriptorLayoutElement
{
    u32 m_BindingID : 3;
    DescriptorType m_Type : 4;
    ShaderStage m_TargetShader : 4;
    u32 m_ArraySize : 16;
};

struct DescriptorSetLayout : APIObject<VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT>
{
    VkDescriptorSetLayout m_pHandle = VK_NULL_HANDLE;
};

struct DescriptorWriteData
{
    DescriptorWriteData(u32 binding, u32 count, Buffer *pBuffer, u32 offset = 0, u32 dataSize = ~0);
    DescriptorWriteData(u32 binding, u32 count, Image *pImage, ImageLayout layout);
    DescriptorWriteData(u32 binding, u32 count, Sampler *pSampler);

    u32 m_Binding = 0;
    u32 m_Count = 1;
    DescriptorType m_Type = LR_DESCRIPTOR_TYPE_COUNT;

    union
    {
        struct
        {
            Buffer *m_pBuffer = nullptr;
            u32 m_BufferDataOffset = 0;
            u32 m_BufferDataSize = ~0;
        };

        struct
        {
            Image *m_pImage;
            ImageLayout m_ImageLayout;
        };

        Sampler *m_pSampler;
    };
};

struct DescriptorSet : APIObject<VK_OBJECT_TYPE_DESCRIPTOR_SET>
{
    VkDescriptorSet m_pHandle = nullptr;
    DescriptorSetLayout *m_pBoundLayout = nullptr;
};

struct DescriptorPoolDesc
{
    DescriptorType m_Type;
    u32 m_Count;
};

struct DescriptorLayoutCache
{
    struct CachedLayout
    {
        u64 m_Hash = 0;
        DescriptorSetLayout *m_pHandle = nullptr;
    };

    DescriptorSetLayout *Get(eastl::span<DescriptorLayoutElement> elements, u64 &hashOut);
    void Add(DescriptorSetLayout *pLayout, u64 hash);

    eastl::vector<CachedLayout> m_Layouts = {};
};

/// ATTACHMENTS ///

union ColorClearValue
{
    ColorClearValue(){};
    ColorClearValue(f32 r, f32 g, f32 b, f32 a) : m_ValFloat({ r, g, b, a }){};
    ColorClearValue(u32 r, u32 g, u32 b, u32 a) : m_ValUInt({ r, g, b, a }){};
    ColorClearValue(i32 r, i32 g, i32 b, i32 a) : m_ValInt({ r, g, b, a }){};
    ColorClearValue(const XMFLOAT4 &val) : m_ValFloat(val){};
    ColorClearValue(const XMUINT4 &val) : m_ValUInt(val){};
    ColorClearValue(const XMINT4 &val) : m_ValInt(val){};

    XMFLOAT4 m_ValFloat = {};
    XMUINT4 m_ValUInt;
    XMINT4 m_ValInt;
};

struct DepthClearValue
{
    DepthClearValue(){};
    DepthClearValue(float depth, u8 stencil) : m_Depth(depth), m_Stencil(stencil){};

    float m_Depth;
    u8 m_Stencil;
};

enum MemoryAcces : u32
{
    LR_MEMORY_ACCESS_NONE = 0,
    LR_MEMORY_ACCESS_VERTEX_ATTRIB_READ = 1 << 0,
    LR_MEMORY_ACCESS_INDEX_ATTRIB_READ = 1 << 1,
    LR_MEMORY_ACCESS_SHADER_READ = 1 << 2,
    LR_MEMORY_ACCESS_SHADER_WRITE = 1 << 3,
    LR_MEMORY_ACCESS_RENDER_TARGET_READ = 1 << 4,
    LR_MEMORY_ACCESS_RENDER_TARGET_WRITE = 1 << 5,
    LR_MEMORY_ACCESS_DEPTH_STENCIL_READ = 1 << 6,
    LR_MEMORY_ACCESS_DEPTH_STENCIL_WRITE = 1 << 7,
    LR_MEMORY_ACCESS_TRANSFER_READ = 1 << 8,
    LR_MEMORY_ACCESS_TRANSFER_WRITE = 1 << 9,
    LR_MEMORY_ACCESS_MEMORY_READ = 1 << 10,
    LR_MEMORY_ACCESS_MEMORY_WRITE = 1 << 11,
    LR_MEMORY_ACCESS_HOST_READ = 1 << 12,
    LR_MEMORY_ACCESS_HOST_WRITE = 1 << 13,
};
EnumFlags(MemoryAcces);

enum AttachmentFlags : u32
{
    LR_ATTACHMENT_FLAG_NONE = 0,
    LR_ATTACHMENT_FLAG_SC_SCALING = 1 << 0,  // Attachment's size is relative to swapchain size
    LR_ATTACHMENT_FLAG_SC_FORMAT = 1 << 1,   // Attachment's format is relative to swapchain format
    LR_ATTACHMENT_FLAG_CLEAR = 1 << 2,
};
EnumFlags(AttachmentFlags);

using ResourceID = u64;
struct ColorAttachment
{
    union
    {
        Image *m_pImage = nullptr;
        ResourceID m_ResourceID;
    };

    AttachmentFlags m_Flags = LR_ATTACHMENT_FLAG_NONE;
    MemoryAcces m_AccessFlags = LR_MEMORY_ACCESS_NONE;
    ColorClearValue m_ClearValue = {};
};

struct DepthAttachment
{
    union
    {
        Image *m_pImage = nullptr;
        ResourceID m_ResourceID;
    };

    AttachmentFlags m_Flags = LR_ATTACHMENT_FLAG_NONE;
    MemoryAcces m_AccessFlags = LR_MEMORY_ACCESS_NONE;
    DepthClearValue m_ClearValue = {};
};

enum BlendFactor : u32
{
    LR_BLEND_FACTOR_ZERO = 0,
    LR_BLEND_FACTOR_ONE,
    LR_BLEND_FACTOR_SRC_COLOR,
    LR_BLEND_FACTOR_INV_SRC_COLOR,
    LR_BLEND_FACTOR_SRC_ALPHA,
    LR_BLEND_FACTOR_INV_SRC_ALPHA,
    LR_BLEND_FACTOR_DST_ALPHA,
    LR_BLEND_FACTOR_INV_DST_ALPHA,
    LR_BLEND_FACTOR_DST_COLOR,
    LR_BLEND_FACTOR_INV_DST_COLOR,
    LR_BLEND_FACTOR_SRC_ALPHA_SAT,
    LR_BLEND_FACTOR_CONSTANT_FACTOR,
    LR_BLEND_FACTOR_INV_CONSTANT_FACTOR,
    LR_BLEND_FACTOR_SRC_1_COLOR,
    LR_BLEND_FACTOR_INV_SRC_1_COLOR,
    LR_BLEND_FACTOR_SRC_1_ALPHA,
    LR_BLEND_FACTOR_INV_SRC_1_ALPHA,
};

enum BlendOp : u32
{
    LR_BLEND_OP_ADD = 0,
    LR_BLEND_OP_SUBTRACT,
    LR_BLEND_OP_REVERSE_SUBTRACT,
    LR_BLEND_OP_MIN,
    LR_BLEND_OP_MAX,
};

enum StencilOp : u32
{
    LR_STENCIL_OP_KEEP = 0,
    LR_STENCIL_OP_ZERO,
    LR_STENCIL_OP_REPLACE,
    LR_STENCIL_OP_INCR_AND_CLAMP,
    LR_STENCIL_OP_DECR_AND_CLAMP,
    LR_STENCIL_OP_INVERT,
    LR_STENCIL_OP_INCR_AND_WRAP,
    LR_STENCIL_OP_DECR_AND_WRAP,
};

enum ColorMask : u32
{
    LR_COLOR_MASK_NONE = 0,
    LR_COLOR_MASK_R = 1 << 0,
    LR_COLOR_MASK_G = 1 << 1,
    LR_COLOR_MASK_B = 1 << 2,
    LR_COLOR_MASK_A = 1 << 3,
};
EnumFlags(ColorMask);

struct ColorBlendAttachment
{
    ColorMask m_WriteMask : 4;
    u32 m_BlendEnable : 1;

    BlendFactor m_SrcBlend : 5;
    BlendFactor m_DstBlend : 5;
    BlendOp m_ColorBlendOp : 4;

    BlendFactor m_SrcBlendAlpha : 5;
    BlendFactor m_DstBlendAlpha : 5;
    BlendOp m_AlphaBlendOp : 4;
};

struct DepthStencilOpDesc
{
    StencilOp m_Pass : 3;
    StencilOp m_Fail : 3;
    CompareOp m_DepthFail : 3;
    CompareOp m_CompareFunc : 3;
};

}  // namespace lr::Graphics
