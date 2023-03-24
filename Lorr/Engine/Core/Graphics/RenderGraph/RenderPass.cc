#include "RenderPass.hh"

namespace lr::Graphics
{
void GraphicsRenderPass::AddColorAttachment(NameID name, MemoryAccess access, AttachmentFlags flags)
{
    ZoneScoped;

    ColorAttachment attachment = {
        .m_Hash = Hash::FNV64String(name),
        .m_Access = access,
        .m_Flags = flags,
    };

    m_ColorAttachments.push_back(attachment);
}
}  // namespace lr::Graphics