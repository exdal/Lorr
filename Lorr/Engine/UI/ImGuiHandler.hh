//
// Created on Saturday 8th October 2022 by e-erdal
//

#pragma once

#include <imgui.h>

#include "Core/Graphics/Vulkan/VKAPI.hh"
#include "Core/Graphics/Vulkan/VKPipeline.hh"

namespace lr::UI
{
    struct ImGuiHandler
    {
        void Init();

        void NewFrame();
        void Render();
        void EndFrame();

        void Deinit();

        Graphics::VKBuffer m_VertexBuffer = {};
        Graphics::VKBuffer m_IndexBuffer = {};

        Graphics::VKImage m_Texture;

        u32 m_VertexCount = 0;
        u32 m_IndexCount = 0;
    };

}  // namespace lr::UI