//
// Created on Saturday 8th October 2022 by e-erdal
//

#pragma once

#include <imgui.h>

namespace lr::UI
{
    struct ImGuiHandler
    {
        void Init(u32 width, u32 height);

        void NewFrame(float width, float height);
        void EndFrame();

        void Destroy();
    };

}  // namespace lr::UI