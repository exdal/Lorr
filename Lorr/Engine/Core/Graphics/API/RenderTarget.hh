//
// Created on Tuesday 10th May 2022 by e-erdal
//

#pragma once

#include "Graphics/API/Vulkan/VKResource.hh"

namespace lr::g
{
    /// OpenGL like "FrameBuffer" handle

    /// Note: This does not handle any memory operations,
    /// you either need to call `Reset` or delete resources manually
    // struct RenderTarget
    // {
    //     RenderTarget &AttachColor(VKResource *pResource);
    //     RenderTarget &AttachDepth(VKResource *pResource);

    //     void Reset(bool deleteAttachments, bool deleteDepthAttachment);

    //     u32 m_Width = 0;   // Width and Height values must be set by GAPI when creating it
    //     u32 m_Height = 0;  //

    //     u32 m_ColorAttachmentCount = 0;
    //     eastl::array<VKResource *, 8> m_ColorAttachments;
    //     VKResource *m_DepthAttachment = nullptr;
    // };

}  // namespace lr