// Created on Thursday May 5th 2022 by exdal
// Last modified on Tuesday May 16th 2023 by exdal

#pragma once

#include "Window/BaseWindow.hh"

namespace lr
{
struct Win32Window : BaseWindow
{
    void Init(const WindowDesc &desc);
    void Poll() override;
    void InitDisplays() override;
    void SetCursor(WindowCursor cursor) override;

    void *m_pInstance = 0;
};

}  // namespace lr