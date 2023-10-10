// Created on Sunday October 9th 2022 by exdal
// Last modified on Monday August 28th 2023 by exdal

#pragma once

#include "Vulkan.hh"  // IWYU pragma: export

namespace lr::Graphics
{
/// BUFFER ---------------------------- ///

enum class BufferUsage : u64
{
    None = 0,
    Vertex = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
    Index = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
    Uniform = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
    TransferSrc = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    TransferDst = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    Storage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
    SamplerDescriptor = VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT,
    ResourceDescriptor = VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT,
};
LR_TYPEOP_ARITHMETIC_INT(BufferUsage, BufferUsage, &);
LR_TYPEOP_ARITHMETIC(BufferUsage, BufferUsage, |);
LR_TYPEOP_ASSIGNMENT(BufferUsage, BufferUsage, |);

/// COMMAND ---------------------------- ///

enum class CommandType : u32
{
    Graphics = 0,
    Compute,
    Transfer,
    Count,
};

/// DESCRIPTOR ---------------------------- ///

enum class DescriptorType : u32
{
    Sampler = VK_DESCRIPTOR_TYPE_SAMPLER,
    SampledImage = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,    // Read only image
    UniformBuffer = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,  // Read only buffer
    StorageImage = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,    // RW image
    StorageBuffer = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,  // RW Buffer
};

enum class DescriptorSetLayoutFlag
{
    DescriptorBuffer = 1 << 0,
    EmbeddedSamplers = 1 << 1,
};
LR_TYPEOP_ARITHMETIC(DescriptorSetLayoutFlag, DescriptorSetLayoutFlag, |);
LR_TYPEOP_ARITHMETIC_INT(DescriptorSetLayoutFlag, DescriptorSetLayoutFlag, &);

/// IMAGE ---------------------------- ///

enum class Format : u32
{
    Unknown = VK_FORMAT_UNDEFINED,
    RGBA8_UNORM = VK_FORMAT_R8G8B8A8_UNORM,
    RGBA8_SRGBF = VK_FORMAT_R8G8B8A8_SRGB,
    BGRA8_UNORM = VK_FORMAT_B8G8R8A8_UNORM,
    RGBA16_SFLOAT = VK_FORMAT_R16G16B16A16_SFLOAT,
    RGBA32_SFLOAD = VK_FORMAT_R32G32B32A32_SFLOAT,
    R32_UINT = VK_FORMAT_R32_UINT,
    R32_SFLOAT = VK_FORMAT_R32_SFLOAT,
    D32_SFLOAT = VK_FORMAT_D32_SFLOAT,
    D32_SFLOAT_S8_UINT = VK_FORMAT_D32_SFLOAT_S8_UINT,
};

constexpr u32 FormatToSize(Format format)
{
    constexpr u32 kFormatSizeLUT[] = {
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

    return kFormatSizeLUT[(u32)format];
}

enum class ImageUsage : u32
{
    Sampled = VK_IMAGE_USAGE_SAMPLED_BIT,
    ColorAttachment = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
    DepthStencilAttachment = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
    TransferSrc = VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
    TransferDst = VK_IMAGE_USAGE_TRANSFER_DST_BIT,
    Storage = VK_IMAGE_USAGE_STORAGE_BIT,
    Present = 1 << 29,     // Virtual flag
    Concurrent = 1 << 30,  // Vulkan's sharing flag

    AspectColor = Sampled | ColorAttachment | Storage,
    AspectDepthStencil = DepthStencilAttachment,
};
LR_TYPEOP_ARITHMETIC_INT(ImageUsage, ImageUsage, &);

enum class ImageLayout : u32
{
    Undefined = VK_IMAGE_LAYOUT_UNDEFINED,
    Present = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    ColorAttachment = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    DepthStencilAttachment = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    ColorReadOnly = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    DepthStencilReadOnly = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
    TransferSrc = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
    TransferDst = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    General = VK_IMAGE_LAYOUT_GENERAL,
};

/// PIPELINE ---------------------------- ///

enum class PipelineStage : u64
{
    None = VK_PIPELINE_STAGE_2_NONE,

    /// ---- IN ORDER ----
    DrawIndirect = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,

    /// GRAPHICS PIPELINE
    VertexAttribInput = VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT,
    IndexAttribInput = VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT,
    VertexShader = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
    TessellationControl = VK_PIPELINE_STAGE_2_TESSELLATION_CONTROL_SHADER_BIT,
    TessellationEvaluation = VK_PIPELINE_STAGE_2_TESSELLATION_EVALUATION_SHADER_BIT,
    PixelShader = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
    EarlyPixelTests = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
    LatePixelTests = VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
    ColorAttachmentOutput = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
    AllGraphics = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,

    /// COMPUTE PIPELINE
    ComputeShader = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,

    /// TRANSFER PIPELINE
    Host = VK_PIPELINE_STAGE_2_HOST_BIT,  // not really in transfer but eh
    Copy = VK_PIPELINE_STAGE_2_COPY_BIT,
    Bilt = VK_PIPELINE_STAGE_2_BLIT_BIT,
    Resolve = VK_PIPELINE_STAGE_2_RESOLVE_BIT,
    Clear = VK_PIPELINE_STAGE_2_CLEAR_BIT,
    AllTransfer = VK_PIPELINE_STAGE_2_TRANSFER_BIT,

    /// OTHER STAGES
    AllCommands = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
    BottomOfPipe = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
};
LR_TYPEOP_ARITHMETIC_INT(PipelineStage, PipelineStage, &);

enum class MemoryAccess : u64
{
    None = VK_ACCESS_2_NONE,
    IndirectRead = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
    VertexAttribRead = VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT,
    IndexAttribRead = VK_ACCESS_2_INDEX_READ_BIT,
    InputAttachmentRead = VK_ACCESS_2_INPUT_ATTACHMENT_READ_BIT,
    UniformRead = VK_ACCESS_2_UNIFORM_READ_BIT,
    SampledRead = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
    StorageRead = VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
    StorageWrite = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
    ColorAttachmentRead = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT,
    ColorAttachmentWrite = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
    DepthStencilRead = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
    DepthStencilWrite = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
    TransferRead = VK_ACCESS_2_TRANSFER_READ_BIT,
    TransferWrite = VK_ACCESS_2_TRANSFER_WRITE_BIT,
    HostRead = VK_ACCESS_2_HOST_READ_BIT,
    HostWrite = VK_ACCESS_2_HOST_WRITE_BIT,
    MemoryRead = VK_ACCESS_2_MEMORY_READ_BIT,
    MemoryWrite = VK_ACCESS_2_MEMORY_WRITE_BIT,

    /// VIRTUAL FLAGS
    Present = VK_ACCESS_2_NONE,

    /// UTILITY FLAGS
    ColorAttachmentAll = ColorAttachmentRead | ColorAttachmentWrite,
    DepthStencilAttachmentAll = DepthStencilRead | DepthStencilWrite,
    TransferAll = TransferRead | TransferWrite,
    HostAll = HostRead | HostWrite,

    ImageReadUTL =
        SampledRead | StorageRead | ColorAttachmentRead | DepthStencilRead | MemoryRead,
    ImageWriteUTL = StorageWrite | ColorAttachmentWrite | DepthStencilWrite | MemoryWrite,

};
LR_TYPEOP_ARITHMETIC_INT(MemoryAccess, MemoryAccess, &);

enum class PrimitiveType : u32
{
    PointList = VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
    LineList = VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
    LineStrip = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP,
    TriangleList = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    TriangleStrip = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
    Patch = VK_PRIMITIVE_TOPOLOGY_PATCH_LIST,
};

enum class CullMode : u32
{
    None = VK_CULL_MODE_NONE,
    Front = VK_CULL_MODE_FRONT_BIT,
    Back = VK_CULL_MODE_BACK_BIT,
};

enum class FillMode : u32
{
    Fill = 0,
    Wireframe,
};

enum class Filtering : u32
{
    Nearest = VK_FILTER_NEAREST,
    Linear = VK_FILTER_LINEAR,
};

enum class TextureAddressMode : u32
{
    Repeat = VK_SAMPLER_ADDRESS_MODE_REPEAT,
    MirroredRepeat = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
    ClampToEdge = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
    ClampToBorder = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
};

enum class CompareOp : u32
{
    Never = VK_COMPARE_OP_NEVER,
    Less = VK_COMPARE_OP_LESS,
    Equal = VK_COMPARE_OP_EQUAL,
    LessEqual = VK_COMPARE_OP_LESS_OR_EQUAL,
    Greater = VK_COMPARE_OP_GREATER,
    NotEqual = VK_COMPARE_OP_NOT_EQUAL,
    GreaterEqual = VK_COMPARE_OP_GREATER_OR_EQUAL,
    Always = VK_COMPARE_OP_ALWAYS,
};

union ColorClearValue
{
    ColorClearValue(){};
    ColorClearValue(f32 r, f32 g, f32 b, f32 a)
        : m_ValFloat({ r, g, b, a }){};
    ColorClearValue(u32 r, u32 g, u32 b, u32 a)
        : m_ValUInt({ r, g, b, a }){};
    ColorClearValue(i32 r, i32 g, i32 b, i32 a)
        : m_ValInt({ r, g, b, a }){};
    ColorClearValue(const XMFLOAT4 &val)
        : m_ValFloat(val){};
    ColorClearValue(const XMUINT4 &val)
        : m_ValUInt(val){};
    ColorClearValue(const XMINT4 &val)
        : m_ValInt(val){};

    XMFLOAT4 m_ValFloat = {};
    XMUINT4 m_ValUInt;
    XMINT4 m_ValInt;
};

struct DepthClearValue
{
    DepthClearValue(){};
    DepthClearValue(float depth, u8 stencil)
        : m_Depth(depth),
          m_Stencil(stencil){};

    float m_Depth;
    u8 m_Stencil;
};

enum class BlendFactor : u32
{
    Zero = 0,
    One,
    SrcColor,
    InvSrcColor,
    SrcAlpha,
    InvSrcAlpha,
    DstAlpha,
    InvDstAlpha,
    DstColor,
    InvDstColor,
    SrcAlphaSat,
    ConstantFactor,
    InvConstantFactor,
    Src1MinusColor,
    InvSrc1MinusColor,
    Src1MinusAlpha,
    InvSrc1MinusAlpha,
};

enum class BlendOp : u32
{
    Add = 0,
    Subtract,
    ReverseSubtract,
    Min,
    Max,
};

enum class StencilOp : u32
{
    Keep = 0,
    Zero,
    Replace,
    IncrAndClamp,
    DecrAndClamp,
    Invert,
    IncrAndWrap,
    DecrAndWrap,
};

enum class ColorMask : u32
{
    None = 0,
    R = 1 << 0,
    G = 1 << 1,
    B = 1 << 2,
    A = 1 << 3,

    RGBA = R | G | B | A,
};
LR_TYPEOP_ARITHMETIC(ColorMask, ColorMask, |);

enum class ShaderStage : u32
{
    Vertex = VK_SHADER_STAGE_VERTEX_BIT,
    Pixel = VK_SHADER_STAGE_FRAGMENT_BIT,
    Compute = VK_SHADER_STAGE_COMPUTE_BIT,
    TessellationControl = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
    TessellationEvaluation = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
    All = Vertex | Pixel | Compute | TessellationControl | TessellationEvaluation,
    Count = 5,
};
LR_TYPEOP_ARITHMETIC_INT(ShaderStage, ShaderStage, &);

namespace Limits
{
    constexpr usize MaxPushConstants = 8;
    constexpr usize MaxVertexAttribs = 8;
    constexpr usize MaxColorAttachments = 8;
    constexpr usize MaxFrameCount = 8;

}  // namespace Limits
}  // namespace lr::Graphics
