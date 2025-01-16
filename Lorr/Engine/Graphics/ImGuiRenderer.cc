#include "Engine/Graphics/ImGuiRenderer.hh"

#include "Engine/Core/Application.hh"

#include "Engine/Graphics/VulkanDevice.hh"

#include <ImGuizmo.h>
#include <SDL2/SDL_mouse.h>

namespace lr {
auto ImGuiRenderer::init(this ImGuiRenderer &self, Device *device) -> void {
    ZoneScoped;

    self.device = device;

    auto &app = Application::get();
    auto shaders_root = app.asset_man.asset_root_path(AssetType::Shader);
    auto fonts_root = app.asset_man.asset_root_path(AssetType::Font);
    auto roboto_path = (fonts_root / "Roboto-Regular.ttf").string();
    auto fa_solid_900_path = (fonts_root / "fa-solid-900.ttf").string();

    //  ── IMGUI CONTEXT ───────────────────────────────────────────────────
    ImGui::CreateContext();
    auto &imgui = ImGui::GetIO();
    imgui.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    imgui.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    imgui.IniFilename = "editor_layout.ini";
    imgui.DisplayFramebufferScale = { 1.0f, 1.0f };
    imgui.BackendFlags = ImGuiBackendFlags_RendererHasVtxOffset;
    imgui.BackendFlags = ImGuiBackendFlags_HasMouseCursors;
    ImGui::StyleColorsDark();

    //  ── FONT ATLAS ──────────────────────────────────────────────────────
    ImWchar icons_ranges[] = { 0xf000, 0xf8ff, 0 };
    ImFontConfig font_config;
    font_config.GlyphMinAdvanceX = 16.0f;
    font_config.MergeMode = true;
    font_config.PixelSnapH = true;

    imgui.Fonts->AddFontFromFileTTF(roboto_path.c_str(), 16.0f, nullptr);
    imgui.Fonts->AddFontFromFileTTF(fa_solid_900_path.c_str(), 14.0f, &font_config, icons_ranges);
    imgui.Fonts->Build();

    u8 *font_data = nullptr;
    i32 font_width, font_height;
    imgui.Fonts->GetTexDataAsRGBA32(&font_data, &font_width, &font_height);
    auto font_bytes = ls::span(font_data, font_width * font_height * 4);

    // clang-format off
    auto &transfer_man = device->transfer_man();
    self.font_atlas_image = Image::create(
        *device,
        vuk::Format::eR8G8B8A8Unorm,
        vuk::ImageUsageFlagBits::eSampled,
        vuk::ImageType::e2D,
        vuk::Extent3D(font_width, font_height, 1u))
        .value();
    self.font_atlas_view = ImageView::create(
        *device,
        self.font_atlas_image,
        vuk::ImageUsageFlagBits::eSampled,
        vuk::ImageViewType::e2D,
        { .aspectMask = vuk::ImageAspectFlagBits::eColor })
        .value();
    transfer_man.upload_staging(self.font_atlas_view, font_bytes);
    IM_FREE(font_data);
    self.pipeline = Pipeline::create(*device, {
        .module_name = "imgui",
        .root_path = shaders_root,
        .shader_path = shaders_root / "imgui.slang",
        .entry_points = { "vs_main", "fs_main" },
    }).value();
    // clang-format on
}

auto ImGuiRenderer::add_image(this ImGuiRenderer &self, vuk::Value<vuk::ImageAttachment> &&attachment) -> ImTextureID {
    ZoneScoped;

    self.rendering_attachments.emplace_back(std::move(attachment));
    return self.rendering_attachments.size();
}

auto ImGuiRenderer::add_image(this ImGuiRenderer &self, ImageView &image_view) -> ImTextureID {
    ZoneScoped;

    auto attachment_info = image_view.get_attachment(*self.device, vuk::ImageUsageFlagBits::eSampled);
    auto attachment = vuk::acquire_ia("imgui_rendering_image_attachment", attachment_info, vuk::Access::eFragmentSampled);
    return self.add_image(std::move(attachment));
}

auto ImGuiRenderer::begin_frame(this ImGuiRenderer &self, f64 delta_time, const vuk::Extent3D &extent) -> void {
    ZoneScoped;

    auto &app = Application::get();
    auto &imgui = ImGui::GetIO();
    imgui.DeltaTime = static_cast<f32>(delta_time);
    imgui.DisplaySize = ImVec2(static_cast<f32>(extent.width), static_cast<f32>(extent.height));

    self.rendering_attachments.clear();
    ImGui::NewFrame();
    ImGuizmo::BeginFrame();

    if (imgui.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange) {
        return;
    }

    auto imgui_cursor = ImGui::GetMouseCursor();
    if (imgui.MouseDrawCursor || imgui_cursor == ImGuiMouseCursor_None) {
        app.window.show_cursor(false);
    } else {
        auto next_cursor = WindowCursor::Arrow;
        // clang-format off
        switch (imgui_cursor) {
            case ImGuiMouseCursor_Arrow: next_cursor = WindowCursor::Arrow; break;
            case ImGuiMouseCursor_TextInput: next_cursor = WindowCursor::TextInput; break;
            case ImGuiMouseCursor_ResizeAll: next_cursor = WindowCursor::ResizeAll; break;
            case ImGuiMouseCursor_ResizeNS: next_cursor = WindowCursor::ResizeNS; break;
            case ImGuiMouseCursor_ResizeEW: next_cursor = WindowCursor::ResizeEW; break;
            case ImGuiMouseCursor_ResizeNESW: next_cursor = WindowCursor::ResizeNESW; break;
            case ImGuiMouseCursor_ResizeNWSE: next_cursor = WindowCursor::ResizeNWSE; break;
            case ImGuiMouseCursor_Hand: next_cursor = WindowCursor::Hand; break;
            case ImGuiMouseCursor_NotAllowed: next_cursor = WindowCursor::NotAllowed; break;
            default: break;
        }
        // clang-format on
        app.window.show_cursor(true);

        if (app.window.get_cursor() != next_cursor) {
            app.window.set_cursor(next_cursor);
        }
    }
}

auto ImGuiRenderer::end_frame(this ImGuiRenderer &self, vuk::Value<vuk::ImageAttachment> &&attachment) -> vuk::Value<vuk::ImageAttachment> {
    ZoneScoped;

    ImGui::Render();

    auto &transfer_man = self.device->transfer_man();
    ImDrawData *draw_data = ImGui::GetDrawData();
    u64 vertex_size_bytes = draw_data->TotalVtxCount * sizeof(ImDrawVert);
    u64 index_size_bytes = draw_data->TotalIdxCount * sizeof(ImDrawIdx);
    if (!draw_data || vertex_size_bytes == 0) {
        return std::move(attachment);
    }

    auto vertex_buffer = transfer_man.alloc_transient_buffer(vuk::MemoryUsage::eGPUtoCPU, vertex_size_bytes);
    auto index_buffer = transfer_man.alloc_transient_buffer(vuk::MemoryUsage::eGPUtoCPU, index_size_bytes);
    auto vertex_data = vertex_buffer.host_ptr<ImDrawVert>();
    auto index_data = index_buffer.host_ptr<ImDrawIdx>();
    for (const auto *draw_list : draw_data->CmdLists) {
        memcpy(vertex_data, draw_list->VtxBuffer.Data, draw_list->VtxBuffer.Size * sizeof(ImDrawVert));
        memcpy(index_data, draw_list->IdxBuffer.Data, draw_list->IdxBuffer.Size * sizeof(ImDrawIdx));
        index_data += draw_list->IdxBuffer.Size;
        vertex_data += draw_list->VtxBuffer.Size;
    }

    auto imgui_pass = vuk::make_pass(
        "imgui",
        [draw_data,
         font_atlas_view = self.device->image_view(self.font_atlas_view.id()),
         vertices = vertex_buffer.buffer,
         indices = index_buffer.buffer,
         &pipeline = *self.device->pipeline(self.pipeline.id())](
            vuk::CommandBuffer &cmd_list,
            VUK_IA(vuk::Access::eColorWrite) dst,
            VUK_ARG(vuk::ImageAttachment[], vuk::Access::eFragmentSampled) rendering_images) {
            struct PushConstants {
                glm::vec2 translate = {};
                glm::vec2 scale = {};
            };

            PushConstants c = {};
            c.scale = { 2.0f / draw_data->DisplaySize.x, 2.0f / draw_data->DisplaySize.y };
            c.translate = { -1.0f - draw_data->DisplayPos.x * c.scale.x, -1.0f - draw_data->DisplayPos.y * c.scale.y };

            cmd_list  //
                .set_dynamic_state(vuk::DynamicStateFlagBits::eViewport | vuk::DynamicStateFlagBits::eScissor)
                .set_rasterization(vuk::PipelineRasterizationStateCreateInfo{})
                .set_color_blend(dst, vuk::BlendPreset::eAlphaBlend)
                .set_viewport(0, vuk::Rect2D::framebuffer())
                .bind_graphics_pipeline(pipeline)
                .bind_index_buffer(indices, sizeof(ImDrawIdx) == 2 ? vuk::IndexType::eUint16 : vuk::IndexType::eUint32)
                .bind_vertex_buffer(
                    0, vertices, 0, vuk::Packed{ vuk::Format::eR32G32Sfloat, vuk::Format::eR32G32Sfloat, vuk::Format::eR8G8B8A8Unorm })
                .push_constants(vuk::ShaderStageFlagBits::eVertex, 0, c);

            ImVec2 clip_off = draw_data->DisplayPos;
            ImVec2 clip_scale = draw_data->FramebufferScale;
            u32 vertex_offset = 0;
            u32 index_offset = 0;
            for (ImDrawList *draw_list : draw_data->CmdLists) {
                for (i32 cmd_i = 0; cmd_i < draw_list->CmdBuffer.Size; cmd_i++) {
                    ImDrawCmd &im_cmd = draw_list->CmdBuffer[cmd_i];
                    ImVec4 clip_rect(
                        (im_cmd.ClipRect.x - clip_off.x) * clip_scale.x,
                        (im_cmd.ClipRect.y - clip_off.y) * clip_scale.y,
                        (im_cmd.ClipRect.z - clip_off.x) * clip_scale.x,
                        (im_cmd.ClipRect.w - clip_off.y) * clip_scale.y);

                    auto pass_extent = cmd_list.get_ongoing_render_pass().extent;
                    auto fb_scale = ImVec2(static_cast<f32>(pass_extent.width), static_cast<f32>(pass_extent.height));
                    if (clip_rect.x < fb_scale.x && clip_rect.y < fb_scale.y && clip_rect.z >= 0.0f && clip_rect.w >= 0.0f) {
                        if (clip_rect.x < 0.0f) {
                            clip_rect.x = 0.0f;
                        }
                        if (clip_rect.y < 0.0f) {
                            clip_rect.y = 0.0f;
                        }

                        vuk::Rect2D scissor;
                        scissor.offset.x = (int32_t)(clip_rect.x);
                        scissor.offset.y = (int32_t)(clip_rect.y);
                        scissor.extent.width = (uint32_t)(clip_rect.z - clip_rect.x);
                        scissor.extent.height = (uint32_t)(clip_rect.w - clip_rect.y);
                        cmd_list.set_scissor(0, scissor);

                        cmd_list.bind_sampler(0, 0, { .magFilter = vuk::Filter::eLinear, .minFilter = vuk::Filter::eLinear });
                        if (im_cmd.TextureId != 0) {
                            auto index = im_cmd.TextureId - 1;
                            cmd_list.bind_image(0, 1, rendering_images[index]);
                        } else {
                            cmd_list.bind_image(0, 1, *font_atlas_view);
                        }

                        cmd_list.draw_indexed(
                            im_cmd.ElemCount, 1, im_cmd.IdxOffset + index_offset, i32(im_cmd.VtxOffset + vertex_offset), 0);
                    }
                }

                vertex_offset += draw_list->VtxBuffer.Size;
                index_offset += draw_list->IdxBuffer.Size;
            }

            return dst;
        });

    auto imgui_rendering_images_arr = vuk::declare_array("imgui_rendering_images", std::span(self.rendering_attachments));

    return imgui_pass(std::move(attachment), std::move(imgui_rendering_images_arr));
}

auto ImGuiRenderer::on_mouse_pos(this ImGuiRenderer &, glm::vec2 pos) -> void {
    ZoneScoped;

    auto &imgui = ImGui::GetIO();
    imgui.AddMousePosEvent(pos.x, pos.y);
}

auto ImGuiRenderer::on_mouse_button(this ImGuiRenderer &, u8 button, bool down) -> void {
    ZoneScoped;

    i32 imgui_button = 0;
    // clang-format off
    switch (button) {
        case SDL_BUTTON_LEFT: imgui_button = 0; break;
        case SDL_BUTTON_RIGHT: imgui_button = 1; break;
        case SDL_BUTTON_MIDDLE: imgui_button = 2; break;
        case SDL_BUTTON_X1: imgui_button = 3; break;
        case SDL_BUTTON_X2: imgui_button = 4; break;
        default: return;
    }
    // clang-format on

    auto &imgui = ImGui::GetIO();
    imgui.AddMouseButtonEvent(imgui_button, down);
}

auto ImGuiRenderer::on_mouse_scroll(this ImGuiRenderer &, glm::vec2 offset) -> void {
    ZoneScoped;

    auto &imgui = ImGui::GetIO();
    imgui.AddMouseWheelEvent(offset.x, offset.y);
}

ImGuiKey to_imgui_key(SDL_Keycode keycode);
auto ImGuiRenderer::on_key(this ImGuiRenderer &, u32 key_code, u32, u16 mods, bool down) -> void {
    ZoneScoped;

    auto &imgui = ImGui::GetIO();
    imgui.AddKeyEvent(to_imgui_key(static_cast<SDL_Keycode>(key_code)), down);
    imgui.AddKeyEvent(ImGuiMod_Ctrl, (mods & KMOD_CTRL) != 0);
    imgui.AddKeyEvent(ImGuiMod_Shift, (mods & KMOD_SHIFT) != 0);
    imgui.AddKeyEvent(ImGuiMod_Alt, (mods & KMOD_ALT) != 0);
    imgui.AddKeyEvent(ImGuiMod_Super, (mods & KMOD_GUI) != 0);
}

auto ImGuiRenderer::on_text_input(this ImGuiRenderer &, c8 *text) -> void {
    ZoneScoped;

    auto &imgui = ImGui::GetIO();
    imgui.AddInputCharactersUTF8(text);
}

ImGuiKey to_imgui_key(SDL_Keycode keycode) {
    ZoneScoped;

    switch (keycode) {
        case SDLK_TAB:
            return ImGuiKey_Tab;
        case SDLK_LEFT:
            return ImGuiKey_LeftArrow;
        case SDLK_RIGHT:
            return ImGuiKey_RightArrow;
        case SDLK_UP:
            return ImGuiKey_UpArrow;
        case SDLK_DOWN:
            return ImGuiKey_DownArrow;
        case SDLK_PAGEUP:
            return ImGuiKey_PageUp;
        case SDLK_PAGEDOWN:
            return ImGuiKey_PageDown;
        case SDLK_HOME:
            return ImGuiKey_Home;
        case SDLK_END:
            return ImGuiKey_End;
        case SDLK_INSERT:
            return ImGuiKey_Insert;
        case SDLK_DELETE:
            return ImGuiKey_Delete;
        case SDLK_BACKSPACE:
            return ImGuiKey_Backspace;
        case SDLK_SPACE:
            return ImGuiKey_Space;
        case SDLK_RETURN:
            return ImGuiKey_Enter;
        case SDLK_ESCAPE:
            return ImGuiKey_Escape;
        case SDLK_QUOTE:
            return ImGuiKey_Apostrophe;
        case SDLK_COMMA:
            return ImGuiKey_Comma;
        case SDLK_MINUS:
            return ImGuiKey_Minus;
        case SDLK_PERIOD:
            return ImGuiKey_Period;
        case SDLK_SLASH:
            return ImGuiKey_Slash;
        case SDLK_SEMICOLON:
            return ImGuiKey_Semicolon;
        case SDLK_EQUALS:
            return ImGuiKey_Equal;
        case SDLK_LEFTBRACKET:
            return ImGuiKey_LeftBracket;
        case SDLK_BACKSLASH:
            return ImGuiKey_Backslash;
        case SDLK_RIGHTBRACKET:
            return ImGuiKey_RightBracket;
        case SDLK_BACKQUOTE:
            return ImGuiKey_GraveAccent;
        case SDLK_CAPSLOCK:
            return ImGuiKey_CapsLock;
        case SDLK_SCROLLLOCK:
            return ImGuiKey_ScrollLock;
        case SDLK_NUMLOCKCLEAR:
            return ImGuiKey_NumLock;
        case SDLK_PRINTSCREEN:
            return ImGuiKey_PrintScreen;
        case SDLK_PAUSE:
            return ImGuiKey_Pause;
        case SDLK_KP_0:
            return ImGuiKey_Keypad0;
        case SDLK_KP_1:
            return ImGuiKey_Keypad1;
        case SDLK_KP_2:
            return ImGuiKey_Keypad2;
        case SDLK_KP_3:
            return ImGuiKey_Keypad3;
        case SDLK_KP_4:
            return ImGuiKey_Keypad4;
        case SDLK_KP_5:
            return ImGuiKey_Keypad5;
        case SDLK_KP_6:
            return ImGuiKey_Keypad6;
        case SDLK_KP_7:
            return ImGuiKey_Keypad7;
        case SDLK_KP_8:
            return ImGuiKey_Keypad8;
        case SDLK_KP_9:
            return ImGuiKey_Keypad9;
        case SDLK_KP_PERIOD:
            return ImGuiKey_KeypadDecimal;
        case SDLK_KP_DIVIDE:
            return ImGuiKey_KeypadDivide;
        case SDLK_KP_MULTIPLY:
            return ImGuiKey_KeypadMultiply;
        case SDLK_KP_MINUS:
            return ImGuiKey_KeypadSubtract;
        case SDLK_KP_PLUS:
            return ImGuiKey_KeypadAdd;
        case SDLK_KP_ENTER:
            return ImGuiKey_KeypadEnter;
        case SDLK_KP_EQUALS:
            return ImGuiKey_KeypadEqual;
        case SDLK_LCTRL:
            return ImGuiKey_LeftCtrl;
        case SDLK_LSHIFT:
            return ImGuiKey_LeftShift;
        case SDLK_LALT:
            return ImGuiKey_LeftAlt;
        case SDLK_LGUI:
            return ImGuiKey_LeftSuper;
        case SDLK_RCTRL:
            return ImGuiKey_RightCtrl;
        case SDLK_RSHIFT:
            return ImGuiKey_RightShift;
        case SDLK_RALT:
            return ImGuiKey_RightAlt;
        case SDLK_RGUI:
            return ImGuiKey_RightSuper;
        case SDLK_APPLICATION:
            return ImGuiKey_Menu;
        case SDLK_0:
            return ImGuiKey_0;
        case SDLK_1:
            return ImGuiKey_1;
        case SDLK_2:
            return ImGuiKey_2;
        case SDLK_3:
            return ImGuiKey_3;
        case SDLK_4:
            return ImGuiKey_4;
        case SDLK_5:
            return ImGuiKey_5;
        case SDLK_6:
            return ImGuiKey_6;
        case SDLK_7:
            return ImGuiKey_7;
        case SDLK_8:
            return ImGuiKey_8;
        case SDLK_9:
            return ImGuiKey_9;
        case SDLK_a:
            return ImGuiKey_A;
        case SDLK_b:
            return ImGuiKey_B;
        case SDLK_c:
            return ImGuiKey_C;
        case SDLK_d:
            return ImGuiKey_D;
        case SDLK_e:
            return ImGuiKey_E;
        case SDLK_f:
            return ImGuiKey_F;
        case SDLK_g:
            return ImGuiKey_G;
        case SDLK_h:
            return ImGuiKey_H;
        case SDLK_i:
            return ImGuiKey_I;
        case SDLK_j:
            return ImGuiKey_J;
        case SDLK_k:
            return ImGuiKey_K;
        case SDLK_l:
            return ImGuiKey_L;
        case SDLK_m:
            return ImGuiKey_M;
        case SDLK_n:
            return ImGuiKey_N;
        case SDLK_o:
            return ImGuiKey_O;
        case SDLK_p:
            return ImGuiKey_P;
        case SDLK_q:
            return ImGuiKey_Q;
        case SDLK_r:
            return ImGuiKey_R;
        case SDLK_s:
            return ImGuiKey_S;
        case SDLK_t:
            return ImGuiKey_T;
        case SDLK_u:
            return ImGuiKey_U;
        case SDLK_v:
            return ImGuiKey_V;
        case SDLK_w:
            return ImGuiKey_W;
        case SDLK_x:
            return ImGuiKey_X;
        case SDLK_y:
            return ImGuiKey_Y;
        case SDLK_z:
            return ImGuiKey_Z;
        case SDLK_F1:
            return ImGuiKey_F1;
        case SDLK_F2:
            return ImGuiKey_F2;
        case SDLK_F3:
            return ImGuiKey_F3;
        case SDLK_F4:
            return ImGuiKey_F4;
        case SDLK_F5:
            return ImGuiKey_F5;
        case SDLK_F6:
            return ImGuiKey_F6;
        case SDLK_F7:
            return ImGuiKey_F7;
        case SDLK_F8:
            return ImGuiKey_F8;
        case SDLK_F9:
            return ImGuiKey_F9;
        case SDLK_F10:
            return ImGuiKey_F10;
        case SDLK_F11:
            return ImGuiKey_F11;
        case SDLK_F12:
            return ImGuiKey_F12;
        case SDLK_F13:
            return ImGuiKey_F13;
        case SDLK_F14:
            return ImGuiKey_F14;
        case SDLK_F15:
            return ImGuiKey_F15;
        case SDLK_F16:
            return ImGuiKey_F16;
        case SDLK_F17:
            return ImGuiKey_F17;
        case SDLK_F18:
            return ImGuiKey_F18;
        case SDLK_F19:
            return ImGuiKey_F19;
        case SDLK_F20:
            return ImGuiKey_F20;
        case SDLK_F21:
            return ImGuiKey_F21;
        case SDLK_F22:
            return ImGuiKey_F22;
        case SDLK_F23:
            return ImGuiKey_F23;
        case SDLK_F24:
            return ImGuiKey_F24;
        case SDLK_AC_BACK:
            return ImGuiKey_AppBack;
        case SDLK_AC_FORWARD:
            return ImGuiKey_AppForward;
        default:
            break;
    }
    return ImGuiKey_None;
}

}  // namespace lr
