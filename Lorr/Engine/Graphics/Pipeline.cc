#include "Pipeline.hh"

namespace lr::Graphics
{
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
    ColorMask writeMask,
    BlendFactor srcBlend,
    BlendFactor dstBlend,
    BlendOp blendOp,
    BlendFactor srcBlendAlpha,
    BlendFactor dstBlendAlpha,
    BlendOp blendOpAlpha)
{
    this->blendEnable = enabled;
    this->colorWriteMask = (VkColorComponentFlags)writeMask;
    this->srcColorBlendFactor = kBlendFactorLUT[(u32)srcBlend];
    this->dstColorBlendFactor = kBlendFactorLUT[(u32)dstBlend];
    this->colorBlendOp = (VkBlendOp)blendOp;
    this->srcAlphaBlendFactor = kBlendFactorLUT[(u32)srcBlendAlpha];
    this->dstAlphaBlendFactor = kBlendFactorLUT[(u32)dstBlendAlpha];
    this->alphaBlendOp = (VkBlendOp)blendOpAlpha;
}

}  // namespace lr::Graphics