//
// Created on Sunday 13th November 2022 by e-erdal
//

#pragma once

#include "Core/Graphics/RHI/Base/BaseAPI.hh"

namespace lr::Renderer
{
    using namespace Graphics;
    struct ImGuiPass
    {
        void Init(BaseAPI *pAPI);
        void Draw(BaseAPI *pAPI, XMMATRIX &mat2d);

        Pipeline *m_pPipeline = nullptr;
        DescriptorSet *m_pDescriptorSet = nullptr;
        DescriptorSet *m_pSamplerSet = nullptr;

        Image *m_pFontImage = nullptr;

        Buffer *m_pVertexBuffer = nullptr;
        Buffer *m_pIndexBuffer = nullptr;

        u32 m_VertexCount = 0;
        u32 m_IndexCount = 0;
    };

}  // namespace lr::Renderer