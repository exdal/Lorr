//
// Created on Thursday 5th May 2022 by e-erdal
//

#pragma once

#include "Core/Window/BaseWindow.hh"

namespace lr
{
    struct Win32Window : public BaseWindow
    {
        void SetCursor(Cursor cursor) override;
        void Poll() override;

        void NativeInit(const WindowDesc &desc) override;
        void GetDisplays() override;

        static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

        HINSTANCE m_Instance = 0;
    };

}  // namespace lr