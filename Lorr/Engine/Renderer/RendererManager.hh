//
// Created on Friday 11th November 2022 by e-erdal
//

#pragma once

#include "Core/Graphics/RHI/Base/BaseAPI.hh"
#include "Core/Graphics/Camera.hh"

#include "Core/Window/BaseWindow.hh"

#include "Pass/ImGuiPass.hh"

namespace lr::Renderer
{
    // * Note:
    // * Passes:
    // *    - Pipeline barriers must happen outside of pass.
    // *    - We do not use interfaces here because of laziness.
    // *    - Passes are allowed to use a global 3D and 2D cameras, they will be passed as an argument
    // *      in their `Begin` function.
    // * Cameras:
    // *    - They can be modified and updated before target pass' `Begin` function.

    using namespace Graphics;
    struct RendererManager
    {
        void Init(APIType type, APIFlags flags, BaseWindow *pWindow);
        void Shutdown();

        void Resize(u32 width, u32 height);

        void Begin();
        void End();

        void InitPasses();

        BaseAPI *m_pAPI = nullptr;

        Camera3D m_Camera3D;
        Camera2D m_Camera2D;

        ImGuiPass m_ImGuiPass;
    };

}  // namespace lr::Renderer