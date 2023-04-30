#include "RenderPass.hh"

namespace lr::Graphics
{
void GraphicsRenderPass::SetColorAttachment(NameID name, MemoryAccess access, AttachmentFlags flags)
{
    ZoneScoped;

    RenderPassAttachment attachment = {
        .m_Hash = Hash::FNV64String(name),
        .m_Access = access,
        .m_Flags = flags,
    };

    m_InputAttachments.push_back(attachment);
}

void GraphicsRenderPass::SetBlendAttachment(const ColorBlendAttachment &attachment)
{
    ZoneScoped;

    m_BlendAttachments.push_back(attachment);
}

void GraphicsRenderPass::SetShader(Shader *pShader)
{
    ZoneScoped;

    m_pPipelineInfo->m_Shaders.push_back(pShader);
}

void GraphicsRenderPass::SetLayout(DescriptorSetLayout *pLayout)
{
    ZoneScoped;

    m_pPipelineInfo->m_Layouts.push_back(pLayout);
}

void GraphicsRenderPass::SetPushConstant(const PushConstantDesc &pushConstant)
{
    ZoneScoped;

    m_pPipelineInfo->m_PushConstants.push_back(pushConstant);
}

void GraphicsRenderPass::SetInputLayout(const InputLayout &layout)
{
    ZoneScoped;

    m_pPipelineInfo->m_InputLayout = layout;
}

}  // namespace lr::Graphics