#include "RenderPass.hh"

namespace lr::Graphics
{
void GraphicsRenderPass::AddColorAttachment(eastl::string_view imageName, const ColorAttachment &attachment)
{
    ZoneScoped;

    assert(!(m_ColorAttachments.full() && "Color attachment limit reached for a renderpass."));

    ColorAttachment &newAttachment = m_ColorAttachments.push_back();
    newAttachment = attachment;
    newAttachment.m_ResourceID = Hash::FNV64String(imageName);
}

void GraphicsRenderPass::AddDepthAttachment(eastl::string_view imageName, const DepthAttachment &attachment)
{
    ZoneScoped;

    m_DepthAttachment = attachment;
    m_DepthAttachment.m_ResourceID = Hash::FNV64String(imageName);
}

eastl::span<ColorAttachment> GraphicsRenderPass::GetColorAttachments()
{
    ZoneScoped;

    return m_ColorAttachments;
}

DepthAttachment *GraphicsRenderPass::GetDepthAttachment()
{
    ZoneScoped;

    return &m_DepthAttachment;
}
}  // namespace lr::Graphics