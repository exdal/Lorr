#pragma once

#include "Engine/Graphics/Vulkan.hh"

#include <SDL3/SDL_events.h>
#include <imgui.h>
#include <implot.h>

namespace lr {
struct ImGuiRenderer {
    static constexpr auto MODULE_NAME = "ImGui Renderer";

    Pipeline pipeline = {};
    Image font_image = {};
    ImageView font_image_view = {};

    std::vector<vuk::Value<vuk::ImageAttachment>> rendering_images = {};
    ankerl::unordered_dense::map<ImageViewID, ImTextureID> acquired_images = {};

    auto init(this ImGuiRenderer &) -> bool;
    auto destroy(this ImGuiRenderer &) -> void;

    auto add_font(this ImGuiRenderer &, const fs::path &path) -> ImFont *;
    auto add_image(this ImGuiRenderer &, vuk::Value<vuk::ImageAttachment> &&attachment) -> ImTextureID;
    auto add_image(this ImGuiRenderer &, ImageView &image_view, LR_THISCALL) -> ImTextureID;

    auto begin_frame(this ImGuiRenderer &, f64 delta_time, const vuk::Extent3D &extent) -> void;
    auto end_frame(this ImGuiRenderer &, vuk::Value<vuk::ImageAttachment> &&attachment) -> vuk::Value<vuk::ImageAttachment>;

    auto window_event(this ImGuiRenderer &, SDL_Event &e) -> void;
    auto on_mouse_pos(this ImGuiRenderer &, glm::vec2 pos) -> void;
    auto on_mouse_button(this ImGuiRenderer &, u8 button, bool down) -> void;
    auto on_mouse_scroll(this ImGuiRenderer &, glm::vec2 offset) -> void;
    auto on_key(this ImGuiRenderer &, u32 key_code, u32 scan_code, u16 mods, bool down) -> void;
    auto on_text_input(this ImGuiRenderer &, const c8 *text) -> void;
};
} // namespace lr
