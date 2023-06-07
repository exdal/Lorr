// Created on Wednesday September 21st 2022 by exdal
// Last modified on Tuesday June 6th 2023 by exdal

#pragma once

#include "APIAllocator.hh"
#include "Config.hh"

#include "Descriptor.hh"
#include "Shader.hh"

namespace lr::Graphics
{
enum class PrimitiveType : u32
{
    PointList = 0,
    LineList,
    LineStrip,
    TriangleList,
    TriangleStrip,
    Patch,
};

enum class CullMode : u32
{
    None = 0,
    Front,
    Back,
};

enum class FillMode : u32
{
    Fill = 0,
    Wireframe,
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
EnumFlags(ColorMask);

struct ColorBlendAttachment : VkPipelineColorBlendAttachmentState
{
    ColorBlendAttachment(
        bool enabled,
        ColorMask writeMask = ColorMask::RGBA,
        BlendFactor srcBlend = BlendFactor::SrcAlpha,
        BlendFactor dstBlend = BlendFactor::InvSrcAlpha,
        BlendOp blendOp = BlendOp::Add,
        BlendFactor srcBlendAlpha = BlendFactor::One,
        BlendFactor dstBlendAlpha = BlendFactor::InvSrcAlpha,
        BlendOp blendOpAlpha = BlendOp::Add);
};

struct DepthStencilOpDesc
{
    StencilOp m_Pass : 3;
    StencilOp m_Fail : 3;
    CompareOp m_DepthFail : 3;
    CompareOp m_CompareFunc : 3;
};

struct PushConstantDesc : VkPushConstantRange
{
    PushConstantDesc(ShaderStage stage, u32 offset, u32 size);
};

struct PipelineLayout : APIObject<VK_OBJECT_TYPE_PIPELINE_LAYOUT>
{
    VkPipelineLayout m_pHandle = nullptr;
};

struct Pipeline : APIObject<VK_OBJECT_TYPE_PIPELINE>
{
    VkPipelineBindPoint m_BindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    VkPipeline m_pHandle = nullptr;
    PipelineLayout *m_pLayout = nullptr;
};

// TODO: Indirect drawing.
// TODO: Instances.
// TODO: MSAA.
struct GraphicsPipelineBuildInfo
{
    eastl::vector<ImageFormat> m_ColorAttachmentFormats = {};
    ImageFormat m_DepthAttachmentFormat = ImageFormat::Unknown;
    eastl::vector<ColorBlendAttachment> m_BlendAttachments = {};
    eastl::vector<Shader *> m_Shaders = {};
    PipelineLayout *m_pLayout = nullptr;
    f32 m_DepthBiasFactor = 0.0;
    f32 m_DepthBiasClamp = 0.0;
    f32 m_DepthSlopeFactor = 0.0;
    DepthStencilOpDesc m_StencilFrontFaceOp = {};
    DepthStencilOpDesc m_StencilBackFaceOp = {};
    u32 m_EnableDepthClamp : 1;
    u32 m_EnableDepthBias : 1;
    u32 m_EnableDepthTest : 1;
    u32 m_EnableDepthWrite : 1;
    u32 m_EnableStencilTest : 1;
    CompareOp m_DepthCompareOp : 3;
    CullMode m_SetCullMode : 2;
    FillMode m_SetFillMode : 1;
    u32 m_FrontFaceCCW : 1;
    u32 m_EnableAlphaToCoverage : 1;
    u32 m_MultiSampleBitCount : 3 = 1;
};

struct ComputePipelineBuildInfo
{
    Shader *m_pShader = nullptr;
    PipelineLayout *m_pLayout = nullptr;
};

}  // namespace lr::Graphics