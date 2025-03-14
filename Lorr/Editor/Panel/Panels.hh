#pragma once

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>

#include "Engine/Graphics/VulkanTypes.hh"

namespace lr {
struct PanelI {
    std::string name = {};
    bool open = true;

    PanelI(std::string name_, bool open_ = true): name(std::move(name_)), open(open_) {};

    virtual ~PanelI() = default;
    virtual auto do_render(vuk::Format format, vuk::Extent3D extent) -> void = 0;
};

} // namespace lr
