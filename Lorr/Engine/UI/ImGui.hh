//
// Created on Saturday 8th October 2022 by exdal
//

#pragma once

#include <imgui.h>

#include "Utils/Timer.hh"

namespace lr::UI
{
    struct ImGuiHandler
    {
        void Init(u32 width, u32 height);

        void NewFrame(f32 width, f32 height);
        void EndFrame();

        void Destroy();

        Timer m_Timer;
        ImGuiMouseCursor m_Cursor = ImGuiMouseCursor_Arrow;
    };

}  // namespace lr::UI