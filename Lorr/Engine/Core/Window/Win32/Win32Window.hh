//
// Created on Thursday 5th May 2022 by e-erdal
//

#pragma once

#include "Window/BaseWindow.hh"

namespace lr
{
    class Win32Window : public BaseWindow
    {
    public:
        void SetCursor(Cursor cursor) override;
        void Poll() override;

    protected:
        void NativeInit(const WindowDesc &desc) override;
        void GetDisplays() override;

    private:
        static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    private:
        HINSTANCE m_Instance = 0;
    };

}  // namespace lr