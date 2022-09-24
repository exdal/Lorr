#include "RenderTarget.hh"

namespace lr::g
{
    // RenderTarget &RenderTarget::AttachColor(VKResource *pResource)
    // {
    //     ZoneScoped;

    //     m_ColorAttachments[m_ColorAttachmentCount++] = pResource;

    //     return *this;
    // }

    // RenderTarget &RenderTarget::AttachDepth(VKResource *pResource)
    // {
    //     ZoneScoped;

    //     m_DepthAttachment = pResource;

    //     return *this;
    // }

    // void RenderTarget::Reset(bool deleteAttachments, bool deleteDepthAttachment)
    // {
    //     ZoneScoped;

    //     if (deleteAttachments)
    //     {
    //         for (u32 i = 0; i < m_ColorAttachmentCount; i++)
    //         {
    //             delete m_ColorAttachments[i];
    //         }
    //     }

    //     if (deleteDepthAttachment)
    //     {
    //         delete m_DepthAttachment;
    //     }

    //     memset(m_ColorAttachments.data(), 0, m_ColorAttachments.count * sizeof(void *));
    //     m_ColorAttachmentCount = 0;
    //     m_DepthAttachment = nullptr;
    // }

}  // namespace lr