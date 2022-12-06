//
// Created on Saturday 8th October 2022 by e-erdal
//

#pragma once

#include <imgui.h>

#include "Core/Graphics/RHI/Base/BaseAPI.hh"
#include "Core/Graphics/Camera.hh"

namespace lr::UI
{
    struct ImGuiRenderer
    {
        void Init(u32 width, u32 height);

        void NewFrame();
        void Render();
        void EndFrame();

        void Destroy();

        Graphics::BasePipeline *m_pPipeline = nullptr;

        Graphics::BaseDescriptorSet *m_pDescriptorSetV = nullptr;
        Graphics::BaseDescriptorSet *m_pDescriptorSetP = nullptr;
        Graphics::BaseBuffer *m_pConstantBufferV = nullptr;

        Graphics::BaseBuffer *m_pVertexBuffer = nullptr;
        Graphics::BaseBuffer *m_pIndexBuffer = nullptr;

        Graphics::BaseImage *m_pTexture = nullptr;

        Graphics::Camera2D m_Camera2D;

        u32 m_VertexCount = 0;
        u32 m_IndexCount = 0;
    };

}  // namespace lr::UI