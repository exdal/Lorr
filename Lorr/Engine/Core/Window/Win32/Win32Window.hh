//
// Created on Thursday 5th May 2022 by exdal
//

#pragma once

#include "Core/Window/BaseWindow.hh"

namespace lr
{
    struct Win32Window : public BaseWindow
    {
        void Init(const WindowDesc &desc);
        void Poll() override;

        void InitDisplays() override;

        void SetCursor(WindowCursor cursor) override;

        static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

        HINSTANCE m_Instance = 0;
    };

}  // namespace lr