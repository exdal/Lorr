// Created on Tuesday May 9th 2023 by exdal
// Last modified on Monday August 28th 2023 by exdal

#include "Pipeline.hh"

namespace lr::Graphics
{
PushConstantDesc::PushConstantDesc(ShaderStage stage, u32 offset, u32 size)
{
    this->stageFlags = (VkShaderStageFlags)stage;
    this->offset = offset;
    this->size = size;
}

constexpr VkBlendFactor kBlendFactorLUT[] = {
    VK_BLEND_FACTOR_ZERO,                      // Zero
    VK_BLEND_FACTOR_ONE,                       // One
    VK_BLEND_FACTOR_SRC_COLOR,                 // SrcColor
    VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR,       // InvSrcColor
    VK_BLEND_FACTOR_SRC_ALPHA,                 // SrcAlpha
    VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,       // InvSrcAlpha
    VK_BLEND_FACTOR_DST_ALPHA,                 // DestAlpha
    VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA,       // InvDestAlpha
    VK_BLEND_FACTOR_DST_COLOR,                 // DestColor
    VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR,       // InvDestColor
    VK_BLEND_FACTOR_SRC_ALPHA_SATURATE,        // SrcAlphaSat
    VK_BLEND_FACTOR_CONSTANT_COLOR,            // ConstantColor
    VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR,  // InvConstantColor
    VK_BLEND_FACTOR_SRC1_COLOR,                // Src1Color
    VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR,      // InvSrc1Color
    VK_BLEND_FACTOR_SRC1_ALPHA,                // Src1Alpha
    VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA,      // InvSrc1Alpha
};

ColorBlendAttachment::ColorBlendAttachment(
    bool enabled,
    ColorMask write_mask,
    BlendFactor src_blend,
    BlendFactor dst_blend,
    BlendOp blend_op,
    BlendFactor src_blend_alpha,
    BlendFactor dst_blend_alpha,
    BlendOp blend_op_alpha)
{
    this->blendEnable = enabled;
    this->colorWriteMask = (VkColorComponentFlags)write_mask;
    this->srcColorBlendFactor = kBlendFactorLUT[(u32)src_blend];
    this->dstColorBlendFactor = kBlendFactorLUT[(u32)dst_blend];
    this->colorBlendOp = (VkBlendOp)blend_op;
    this->srcAlphaBlendFactor = kBlendFactorLUT[(u32)src_blend_alpha];
    this->dstAlphaBlendFactor = kBlendFactorLUT[(u32)dst_blend_alpha];
    this->alphaBlendOp = (VkBlendOp)blend_op_alpha;
}

}  // namespace lr::Graphics