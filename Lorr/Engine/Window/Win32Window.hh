#pragma once

#include "Window.hh"

namespace lr::window {
struct Win32Window : Window {
    bool init(const WindowInfo &info) override;
    void get_displays();
    void poll() override;
    void set_cursor(WindowCursor cursor) override;
    VkSurfaceKHR get_surface(VkInstance instance) override;

    void *m_instance = nullptr;
};

}  // namespace lr::window
