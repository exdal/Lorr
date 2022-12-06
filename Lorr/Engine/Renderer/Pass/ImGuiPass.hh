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

        BaseImage *m_pImage = nullptr;
        BaseSampler *m_pSampler = nullptr;

        BaseBuffer *m_pVertexBuffer = nullptr;
        BaseBuffer *m_pIndexBuffer = nullptr;
    };

}  // namespace lr::Renderer