// Created on Thursday May 5th 2022 by exdal
// Last modified on Tuesday May 16th 2023 by exdal

#pragma once

#include "Window/BaseWindow.hh"

namespace lr
{
struct Win32Window : BaseWindow
{
    void init(const WindowDesc &desc);
    void poll() override;
    void init_displays() override;
    void set_cursor(WindowCursor cursor) override;

    void *m_instance = nullptr;
};

}  // namespace lr