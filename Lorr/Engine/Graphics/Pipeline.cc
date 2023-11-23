#include "Pipeline.hh"

namespace lr::Graphics
{
PushConstantDesc::PushConstantDesc(ShaderStage stage, u32 offset, u32 size)
    : VkPushConstantRange()
{
    this->stageFlags = static_cast<VkShaderStageFlags>(stage);
    this->offset = offset;
    this->size = size;
}
PipelineShaderStageInfo::PipelineShaderStageInfo(Shader *shader, eastl::string_view entry_point)
    : VkPipelineShaderStageCreateInfo(VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO)
{
    this->stage = static_cast<VkShaderStageFlagBits>(shader->m_type);
    this->module = shader->m_handle;
    this->pName = entry_point.data();
}

PipelineAttachmentInfo::PipelineAttachmentInfo(eastl::span<Format> color_attachment_formats, Format depth_attachment_format)
    : VkPipelineRenderingCreateInfo(VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO)
{
    this->colorAttachmentCount = color_attachment_formats.size();
    this->pColorAttachmentFormats = reinterpret_cast<VkFormat *>(color_attachment_formats.data());
    this->depthAttachmentFormat = static_cast<VkFormat>(depth_attachment_format);
}

PipelineVertexLayoutBindingInfo::PipelineVertexLayoutBindingInfo(u32 binding_id, u32 stride, bool is_instanced)
    : VkVertexInputBindingDescription()
{
    this->binding = binding_id;
    this->stride = stride;
    this->inputRate = !is_instanced ? VK_VERTEX_INPUT_RATE_VERTEX : VK_VERTEX_INPUT_RATE_INSTANCE;
}

PipelineVertexAttribInfo::PipelineVertexAttribInfo(u32 target_binding, u32 index, Format format, u32 offset)
    : VkVertexInputAttributeDescription()
{
    this->binding = target_binding;
    this->location = index;
    this->format = static_cast<VkFormat>(format);
    this->offset = offset;
}

ColorBlendAttachment::ColorBlendAttachment(
    bool enabled,
    ColorMask write_mask,
    BlendFactor src_blend,
    BlendFactor dst_blend,
    BlendOp blend_op,
    BlendFactor src_blend_alpha,
    BlendFactor dst_blend_alpha,
    BlendOp blend_op_alpha)
    : VkPipelineColorBlendAttachmentState()
{
    this->blendEnable = enabled;
    this->colorWriteMask = static_cast<VkColorComponentFlags>(write_mask);
    this->srcColorBlendFactor = static_cast<VkBlendFactor>(src_blend);
    this->dstColorBlendFactor = static_cast<VkBlendFactor>(dst_blend);
    this->colorBlendOp = static_cast<VkBlendOp>(blend_op);
    this->srcAlphaBlendFactor = static_cast<VkBlendFactor>(src_blend_alpha);
    this->dstAlphaBlendFactor = static_cast<VkBlendFactor>(dst_blend_alpha);
    this->alphaBlendOp = static_cast<VkBlendOp>(blend_op_alpha);
}

}  // namespace lr::Graphics