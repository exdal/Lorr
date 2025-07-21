#include "Engine/Graphics/ImGuiRenderer.hh"

#include "Engine/Core/Application.hh"

#include "Engine/Graphics/VulkanDevice.hh"

#include <ImGuizmo.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_mouse.h>

#include "Engine/Util/Icons/IconsMaterialDesignIcons.hh"

namespace lr {
auto ImGuiRenderer::init(this ImGuiRenderer &self, Device *device) -> void {
    ZoneScoped;

    self.device = device;

    auto &app = Application::get();
    auto shaders_root = app.asset_man.asset_root_path(AssetType::Shader);
    auto fonts_root = app.asset_man.asset_root_path(AssetType::Font);
    auto roboto_path = (fonts_root / "Roboto-Regular.ttf").string();
    auto materialdesignicons_path = (fonts_root / FONT_ICON_FILE_NAME_MDI).string();

    // ── IMGUI CONTEXT ───────────────────────────────────────────────────
    ImGui::CreateContext();
    auto &imgui = ImGui::GetIO();
    imgui.ConfigWindowsMoveFromTitleBarOnly = true;
    imgui.IniFilename = "editor_layout.ini";
    imgui.DisplayFramebufferScale = { 1.0f, 1.0f };
    imgui.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    imgui.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    imgui.ConfigFlags |= ImGuiConfigFlags_IsSRGB;
    imgui.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
    imgui.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;
    imgui.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
    ImGui::StyleColorsDark();

    //  ── IMPLOT CONTEXT ──────────────────────────────────────────────────
    ImPlot::CreateContext();

    //  ── FONT ATLAS ──────────────────────────────────────────────────────
    ImFontConfig font_config = {};
    font_config.GlyphMinAdvanceX = 16.0f;
    font_config.MergeMode = true;
    font_config.PixelSnapH = true;
    font_config.OversampleH = 0.0f;

    auto font_ranges = ImVector<ImWchar>();
    ImFontGlyphRangesBuilder font_ranges_builder = {};
    const ImWchar ranges[] = { ICON_MIN_MDI, ICON_MAX_MDI, 0 };
    font_ranges_builder.AddRanges(ranges);
    font_ranges_builder.BuildRanges(&font_ranges);

    auto *roboto_font = imgui.Fonts->AddFontFromFileTTF(roboto_path.c_str(), 16.0f);
    imgui.Fonts->AddFontFromFileTTF(materialdesignicons_path.c_str(), 16.0f, &font_config, font_ranges.Data);
    imgui.Fonts->TexDesiredFormat = ImTextureFormat_RGBA32;
    imgui.FontDefault = roboto_font;

    auto slang_session = device->new_slang_session({ .root_directory = shaders_root }).value();
    auto pipeline_info = PipelineCompileInfo{
        .module_name = "passes.imgui",
        .entry_points = { "vs_main", "fs_main" },
    };
    self.pipeline = Pipeline::create(*device, slang_session, pipeline_info).value();
}

auto ImGuiRenderer::destroy(this ImGuiRenderer &self) -> void {
    if (self.font_image_view) {
        self.device->destroy(self.font_image_view.id());
    }

    if (self.font_image) {
        self.device->destroy(self.font_image.id());
    }
}

auto ImGuiRenderer::add_image(this ImGuiRenderer &self, vuk::Value<vuk::ImageAttachment> &&attachment) -> ImTextureID {
    ZoneScoped;

    self.rendering_images.emplace_back(std::move(attachment));
    return self.rendering_images.size();
}

auto ImGuiRenderer::add_image(this ImGuiRenderer &self, ImageView &image_view, LR_CALLSTACK) -> ImTextureID {
    ZoneScoped;

    auto acquired_it = self.acquired_images.find(image_view.id());
    if (acquired_it != self.acquired_images.end()) {
        return acquired_it->second;
    }

    auto attachment =
        image_view.acquire(*self.device, "imgui rendering image", vuk::ImageUsageFlagBits::eSampled, vuk::Access::eFragmentSampled, LOC);
    auto texture_id = self.add_image(std::move(attachment));
    self.acquired_images.emplace(image_view.id(), texture_id);

    return texture_id;
}

auto ImGuiRenderer::begin_frame(this ImGuiRenderer &self, f64 delta_time, const vuk::Extent3D &extent) -> void {
    ZoneScoped;

    auto &app = Application::get();
    auto &imgui = ImGui::GetIO();
    imgui.DeltaTime = static_cast<f32>(delta_time);
    imgui.DisplaySize = ImVec2(static_cast<f32>(extent.width), static_cast<f32>(extent.height));

    self.rendering_images.clear();
    self.acquired_images.clear();

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
    auto *draw_data = ImGui::GetDrawData();

    if (draw_data->Textures) {
        for (auto *texture : *draw_data->Textures) {
            auto acquired = false;
            auto acquired_image = vuk::Value<vuk::ImageAttachment>{};
            auto upload_offset = vuk::Offset3D(texture->UpdateRect.x, texture->UpdateRect.y, 0);
            auto upload_extent = vuk::Extent3D(texture->UpdateRect.w, texture->UpdateRect.h, 1);

            switch (texture->Status) {
                case ImTextureStatus_WantCreate: {
                    auto image_info = ImageInfo{
                        .format = vuk::Format::eR8G8B8A8Srgb,
                        .usage = vuk::ImageUsageFlagBits::eSampled,
                        .type = vuk::ImageType::e2D,
                        .extent = vuk::Extent3D(texture->Width, texture->Height, 1u),
                        .name = "ImGui Font",
                    };
                    std::tie(self.font_image, self.font_image_view) = Image::create_with_view(*self.device, image_info).value();
                    acquired_image = self.font_image_view.acquire(
                        *self.device,
                        "imgui image",
                        vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eTransferDst,
                        vuk::eNone
                    );
                    acquired = true;

                    upload_offset = {};
                    upload_extent = image_info.extent;

                    [[fallthrough]];
                }
                case ImTextureStatus_WantUpdates: {
                    auto buffer_alignment = self.device->non_coherent_atom_size();
                    auto upload_pitch = upload_extent.width * texture->BytesPerPixel;
                    auto buffer_size = ls::align_up(upload_pitch * upload_extent.height, buffer_alignment);
                    auto upload_buffer = transfer_man.alloc_transient_buffer(vuk::MemoryUsage::eCPUonly, buffer_size);
                    auto *buffer_ptr = reinterpret_cast<u8 *>(upload_buffer->mapped_ptr);
                    for (auto y = 0_u32; y < upload_extent.height; y++) {
                        std::memcpy(
                            buffer_ptr + upload_pitch * y,
                            texture->GetPixelsAt(upload_offset.x, upload_offset.y + static_cast<i32>(y)),
                            upload_pitch
                        );
                    }

                    auto upload_pass = vuk::make_pass(
                        "upload",
                        [upload_offset, upload_extent](
                            vuk::CommandBuffer &cmd_list, //
                            VUK_BA(vuk::eTransferRead) src,
                            VUK_IA(vuk::eTransferWrite) dst
                        ) {
                            auto buffer_copy_region = vuk::BufferImageCopy{
                                .bufferOffset = src->offset,
                                .imageSubresource = { .aspectMask = vuk::ImageAspectFlagBits::eColor, .layerCount = 1 },
                                .imageOffset = upload_offset,
                                .imageExtent = upload_extent,
                            };
                            cmd_list.copy_buffer_to_image(src, dst, buffer_copy_region);
                            return dst;
                        }
                    );

                    if (!acquired) {
                        acquired_image = self.font_image_view.acquire(
                            *self.device,
                            "imgui image",
                            vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eTransferDst,
                            vuk::eNone
                        );
                    }

                    acquired_image = upload_pass(std::move(upload_buffer), std::move(acquired_image));
                    auto texture_id = self.add_image(std::move(acquired_image));
                    texture->SetTexID(texture_id);
                    texture->SetStatus(ImTextureStatus_OK);
                } break;
                case ImTextureStatus_OK: {
                    acquired_image =
                        self.font_image_view.acquire(*self.device, "imgui image", vuk::ImageUsageFlagBits::eSampled, vuk::eFragmentSampled);
                    auto texture_id = self.add_image(std::move(acquired_image));
                    texture->SetTexID(texture_id);
                } break;
                case ImTextureStatus_WantDestroy: {
                    self.device->destroy(self.font_image.id());
                    self.device->destroy(self.font_image_view.id());
                } break;
                case ImTextureStatus_Destroyed:;
            }
        }
    }

    u64 vertex_size_bytes = draw_data->TotalVtxCount * sizeof(ImDrawVert);
    u64 index_size_bytes = draw_data->TotalIdxCount * sizeof(ImDrawIdx);
    if (!draw_data || vertex_size_bytes == 0) {
        return std::move(attachment);
    }

    auto vertex_buffer = transfer_man.alloc_transient_buffer(vuk::MemoryUsage::eGPUtoCPU, vertex_size_bytes);
    auto index_buffer = transfer_man.alloc_transient_buffer(vuk::MemoryUsage::eGPUtoCPU, index_size_bytes);
    auto vertex_data = reinterpret_cast<ImDrawVert *>(vertex_buffer->mapped_ptr);
    auto index_data = reinterpret_cast<ImDrawIdx *>(index_buffer->mapped_ptr);
    for (const auto *draw_list : draw_data->CmdLists) {
        memcpy(vertex_data, draw_list->VtxBuffer.Data, draw_list->VtxBuffer.Size * sizeof(ImDrawVert));
        memcpy(index_data, draw_list->IdxBuffer.Data, draw_list->IdxBuffer.Size * sizeof(ImDrawIdx));
        index_data += draw_list->IdxBuffer.Size;
        vertex_data += draw_list->VtxBuffer.Size;
    }

    auto imgui_pass = vuk::make_pass(
        "imgui",
        [draw_data]( //
            vuk::CommandBuffer &cmd_list,
            VUK_IA(vuk::Access::eColorWrite) dst,
            VUK_BA(vuk::Access::eVertexRead) vertex_buf,
            VUK_BA(vuk::Access::eIndexRead) index_buf,
            VUK_ARG(vuk::ImageAttachment[], vuk::Access::eFragmentSampled) rendering_images
        ) {
            auto scale = glm::vec2(2.0f / draw_data->DisplaySize.x, 2.0f / draw_data->DisplaySize.y);
            auto translate = glm::vec2(-1.0f - draw_data->DisplayPos.x * scale.x, -1.0f - draw_data->DisplayPos.y * scale.y);

            cmd_list //
                .set_dynamic_state(vuk::DynamicStateFlagBits::eViewport | vuk::DynamicStateFlagBits::eScissor)
                .set_rasterization(vuk::PipelineRasterizationStateCreateInfo{})
                .set_color_blend(dst, vuk::BlendPreset::eAlphaBlend)
                .set_viewport(0, vuk::Rect2D::framebuffer())
                .bind_graphics_pipeline("passes.imgui")
                .bind_index_buffer(index_buf, sizeof(ImDrawIdx) == 2 ? vuk::IndexType::eUint16 : vuk::IndexType::eUint32)
                .bind_vertex_buffer(
                    0,
                    vertex_buf,
                    0,
                    vuk::Packed{ vuk::Format::eR32G32Sfloat, vuk::Format::eR32G32Sfloat, vuk::Format::eR8G8B8A8Unorm }
                );

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
                        (im_cmd.ClipRect.w - clip_off.y) * clip_scale.y
                    );

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
                        auto index = im_cmd.GetTexID() - 1;
                        const auto &image = rendering_images[index];
                        cmd_list.bind_image(0, 1, image);

                        cmd_list.push_constants(vuk::ShaderStageFlagBits::eVertex, 0, PushConstants(translate, scale));
                        cmd_list.draw_indexed(im_cmd.ElemCount, 1, im_cmd.IdxOffset + index_offset, i32(im_cmd.VtxOffset + vertex_offset), 0);
                    }
                }

                vertex_offset += draw_list->VtxBuffer.Size;
                index_offset += draw_list->IdxBuffer.Size;
            }

            return dst;
        }
    );

    auto imgui_rendering_images_arr = vuk::declare_array("imgui rendering images", std::span(self.rendering_images));

    return imgui_pass(std::move(attachment), std::move(vertex_buffer), std::move(index_buffer), std::move(imgui_rendering_images_arr));
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
    imgui.AddMouseSourceEvent(ImGuiMouseSource_Mouse);
}

auto ImGuiRenderer::on_mouse_scroll(this ImGuiRenderer &, glm::vec2 offset) -> void {
    ZoneScoped;

    auto &imgui = ImGui::GetIO();
    imgui.AddMouseWheelEvent(offset.x, offset.y);
}

ImGuiKey to_imgui_key(SDL_Keycode keycode, SDL_Scancode scancode);
auto ImGuiRenderer::on_key(this ImGuiRenderer &, u32 key_code, u32 scan_code, u16 mods, bool down) -> void {
    ZoneScoped;

    auto &imgui = ImGui::GetIO();
    imgui.AddKeyEvent(ImGuiMod_Ctrl, (mods & SDL_KMOD_CTRL) != 0);
    imgui.AddKeyEvent(ImGuiMod_Shift, (mods & SDL_KMOD_SHIFT) != 0);
    imgui.AddKeyEvent(ImGuiMod_Alt, (mods & SDL_KMOD_ALT) != 0);
    imgui.AddKeyEvent(ImGuiMod_Super, (mods & SDL_KMOD_GUI) != 0);

    auto key = to_imgui_key(static_cast<SDL_Keycode>(key_code), static_cast<SDL_Scancode>(scan_code));
    imgui.AddKeyEvent(key, down);
    imgui.SetKeyEventNativeData(key, static_cast<i32>(key_code), static_cast<i32>(scan_code), static_cast<i32>(scan_code));
}

auto ImGuiRenderer::on_text_input(this ImGuiRenderer &, const c8 *text) -> void {
    ZoneScoped;

    auto &imgui = ImGui::GetIO();
    imgui.AddInputCharactersUTF8(text);
}

ImGuiKey to_imgui_key(SDL_Keycode keycode, SDL_Scancode scancode) {
    ZoneScoped;

    switch (scancode) {
        case SDL_SCANCODE_KP_0:
            return ImGuiKey_Keypad0;
        case SDL_SCANCODE_KP_1:
            return ImGuiKey_Keypad1;
        case SDL_SCANCODE_KP_2:
            return ImGuiKey_Keypad2;
        case SDL_SCANCODE_KP_3:
            return ImGuiKey_Keypad3;
        case SDL_SCANCODE_KP_4:
            return ImGuiKey_Keypad4;
        case SDL_SCANCODE_KP_5:
            return ImGuiKey_Keypad5;
        case SDL_SCANCODE_KP_6:
            return ImGuiKey_Keypad6;
        case SDL_SCANCODE_KP_7:
            return ImGuiKey_Keypad7;
        case SDL_SCANCODE_KP_8:
            return ImGuiKey_Keypad8;
        case SDL_SCANCODE_KP_9:
            return ImGuiKey_Keypad9;
        case SDL_SCANCODE_KP_PERIOD:
            return ImGuiKey_KeypadDecimal;
        case SDL_SCANCODE_KP_DIVIDE:
            return ImGuiKey_KeypadDivide;
        case SDL_SCANCODE_KP_MULTIPLY:
            return ImGuiKey_KeypadMultiply;
        case SDL_SCANCODE_KP_MINUS:
            return ImGuiKey_KeypadSubtract;
        case SDL_SCANCODE_KP_PLUS:
            return ImGuiKey_KeypadAdd;
        case SDL_SCANCODE_KP_ENTER:
            return ImGuiKey_KeypadEnter;
        case SDL_SCANCODE_KP_EQUALS:
            return ImGuiKey_KeypadEqual;
        default:
            break;
    }

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
        case SDLK_APOSTROPHE:
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
        case SDLK_GRAVE:
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
        case SDLK_A:
            return ImGuiKey_A;
        case SDLK_B:
            return ImGuiKey_B;
        case SDLK_C:
            return ImGuiKey_C;
        case SDLK_D:
            return ImGuiKey_D;
        case SDLK_E:
            return ImGuiKey_E;
        case SDLK_F:
            return ImGuiKey_F;
        case SDLK_G:
            return ImGuiKey_G;
        case SDLK_H:
            return ImGuiKey_H;
        case SDLK_I:
            return ImGuiKey_I;
        case SDLK_J:
            return ImGuiKey_J;
        case SDLK_K:
            return ImGuiKey_K;
        case SDLK_L:
            return ImGuiKey_L;
        case SDLK_M:
            return ImGuiKey_M;
        case SDLK_N:
            return ImGuiKey_N;
        case SDLK_O:
            return ImGuiKey_O;
        case SDLK_P:
            return ImGuiKey_P;
        case SDLK_Q:
            return ImGuiKey_Q;
        case SDLK_R:
            return ImGuiKey_R;
        case SDLK_S:
            return ImGuiKey_S;
        case SDLK_T:
            return ImGuiKey_T;
        case SDLK_U:
            return ImGuiKey_U;
        case SDLK_V:
            return ImGuiKey_V;
        case SDLK_W:
            return ImGuiKey_W;
        case SDLK_X:
            return ImGuiKey_X;
        case SDLK_Y:
            return ImGuiKey_Y;
        case SDLK_Z:
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

} // namespace lr
