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

        BasePipeline *m_pPipeline = nullptr;
        BaseDescriptorSet *m_pDescriptorSet = nullptr;
        BaseDescriptorSet *m_pSamplerSet = nullptr;

        BaseImage *m_pFontImage = nullptr;

        BaseBuffer *m_pVertexBuffer = nullptr;
        BaseBuffer *m_pIndexBuffer = nullptr;

        u32 m_VertexCount = 0;
        u32 m_IndexCount = 0;
    };

}  // namespace lr::Renderer