#pragma once

#include "ExampleBase.hh"

#include "Graphics/Common.hh"
#include "Graphics/CommandList.hh"
#include "Graphics/Device.hh"

#include "OS/Key.hh"

#include <imgui.h>

namespace lr::example {
struct ImGuiBackend {
    struct PushConstant {
        glm::vec2 translate;
        glm::vec2 scale;
        graphics::ImageViewID image_view_id;
        graphics::SamplerID sampler_id;
    };

    graphics::PipelineID m_pipeline_id;
    graphics::ImageID m_font_image_id;
    graphics::ImageViewID m_font_image_view_id;
    graphics::SamplerID m_font_sampler_id;

    graphics::BufferID m_vertex_buffer_id;
    graphics::BufferID m_index_buffer_id;

    f64 m_last_time;

    graphics::BufferID recreate_buffer(graphics::Device &device, graphics::BufferUsage usage, u64 data_size)
    {
        return device.create_buffer({ .usage_flags = usage | graphics::BufferUsage::TransferDst, .data_size = data_size });
    }

    bool init(graphics::Device &device, graphics::SwapChain &swap_chain)
    {
        using namespace lr::graphics;

        ImGui::CreateContext();
        auto &io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.IniFilename = nullptr;
        ImGui::StyleColorsDark();

        {
            ShaderCompileInfo info = {
                .virtual_env = { &example::default_vdir(), 1 },
            };

            std::string_view path = IMGUI_SHADER_PATH;
            auto [code, result] = example::load_file(path);
            if (!result) {
                LR_LOG_ERROR("Failed to load imgui shader.");
                return false;
            }

            info.code = code;
            info.real_path = path;
            info.compile_flags = ShaderCompileFlag::InvertY;

            info.entry_point = "vs_main";
            auto [vs_data, vs_result] = ShaderCompiler::compile(info);
            if (!vs_result) {
                return false;
            }

            info.entry_point = "fs_main";
            auto [fs_data, fs_result] = ShaderCompiler::compile(info);
            if (!fs_result) {
                return false;
            }

            ShaderID vs_shader = device.create_shader(ShaderStageFlag::Vertex, vs_data);
            ShaderID fs_shader = device.create_shader(ShaderStageFlag::Pixel, fs_data);

            std::array shader_ids = { vs_shader, fs_shader };
            Viewport viewport = {
                .x = 0,
                .y = 0,
                .width = f32(swap_chain.m_extent.width),
                .height = f32(swap_chain.m_extent.height),
                .depth_min = 0.01,
                .depth_max = 1.0,
            };
            Rect2D scissors = {};
            PipelineColorBlendAttachment blend_attachment = {
                .blend_enabled = true,
                .src_blend = BlendFactor::SrcAlpha,
                .dst_blend = BlendFactor::InvSrcAlpha,
                .src_blend_alpha = BlendFactor::One,
                .dst_blend_alpha = BlendFactor::InvSrcAlpha,
            };

            VertexLayoutBindingInfo layout_binding_info = { .binding = 0, .stride = sizeof(ImDrawVert) };
            VertexAttribInfo vertex_attrib_infos[] = {
                { .format = Format::R32G32_SFLOAT, .location = 0, .offset = offsetof(ImDrawVert, pos) },
                { .format = Format::R32G32_SFLOAT, .location = 1, .offset = offsetof(ImDrawVert, uv) },
                { .format = Format::R8G8B8A8_UNORM, .location = 2, .offset = offsetof(ImDrawVert, col) },
            };

            GraphicsPipelineInfo imgui_pipeline_info = {
                .color_attachment_formats = { &swap_chain.m_format, 1 },
                .viewports = { &viewport, 1 },
                .scissors = { &scissors, 1 },
                .shader_ids = shader_ids,
                .vertex_binding_infos = { &layout_binding_info, 1 },
                .vertex_attrib_infos = vertex_attrib_infos,
                .blend_attachments = { &blend_attachment, 1 },
                .dynamic_state = DynamicState::Viewport | DynamicState::Scissor,
                .layout = device.get_layout<PushConstant>(),
            };
            m_pipeline_id = device.create_graphics_pipeline(imgui_pipeline_info);
        }

        {
            CommandQueue &queue = device.get_queue(CommandType::Graphics);
            Unique<CommandAllocator> temp_callocator(&device);
            CommandList temp_cmd_list;

            device.create_command_allocators({ &*temp_callocator, 1 }, { .type = CommandType::Graphics, .flags = graphics::CommandAllocatorFlag::Transient });
            device.create_command_lists({ &temp_cmd_list, 1 }, *temp_callocator);

            u8 *font_data = nullptr;
            i32 font_width, font_height;
            io.Fonts->GetTexDataAsRGBA32(&font_data, &font_width, &font_height);
            usize upload_size = font_width * font_height * 4 * sizeof(u8);

            BufferID upload_buffer = device.create_buffer({
                .usage_flags = BufferUsage::TransferSrc,
                .flags = MemoryFlag::HostSeqWrite,
                .data_size = upload_size,
            });
            u8 *upload_data = device.buffer_host_data<u8>(upload_buffer);
            memcpy(upload_data, font_data, upload_size);

            m_font_image_id = device.create_image({
                .usage_flags = ImageUsage::TransferDst | ImageUsage::Sampled,
                .format = Format::R8G8B8A8_UNORM,
                .extent = { u32(font_width), u32(font_height), 1 },
            });

            m_font_image_view_id = device.create_image_view({ .image_id = m_font_image_id });

            ImageCopyRegion copy_region = {
                .image_subresource_layer = { .aspect_mask = ImageAspect::Color },
                .image_extent = { u32(font_width), u32(font_height), 1 },
            };

            device.begin_command_list(temp_cmd_list);
            temp_cmd_list.image_transition({
                .src_access = PipelineAccess::None,
                .dst_access = PipelineAccess::TransferWrite,
                .old_layout = ImageLayout::Undefined,
                .new_layout = ImageLayout::TransferDst,
                .image_id = m_font_image_id,
            });
            temp_cmd_list.copy_buffer_to_image(upload_buffer, m_font_image_id, ImageLayout::TransferDst, { &copy_region, 1 });
            temp_cmd_list.image_transition({
                .src_access = PipelineAccess::TransferWrite,
                .dst_access = PipelineAccess::PixelShaderRead,
                .old_layout = ImageLayout::TransferDst,
                .new_layout = ImageLayout::ColorReadOnly,
                .image_id = m_font_image_id,
            });
            device.end_command_list(temp_cmd_list);

            SemaphoreSubmitInfo wait_sema_info(queue.semaphore(), queue.semaphore().counter(), PipelineStage::AllCommands);
            CommandListSubmitInfo cmd_list_info = { temp_cmd_list };
            SemaphoreSubmitInfo signal_sema_info(queue.semaphore(), queue.semaphore().advance(), PipelineStage::PixelShader);
            QueueSubmitInfo submit_info = {
                .wait_sema_count = 1,
                .wait_sema_infos = &wait_sema_info,
                .command_list_count = 1,
                .command_list_infos = &cmd_list_info,
                .signal_sema_count = 1,
                .signal_sema_infos = &signal_sema_info,
            };
            queue.submit(submit_info);

            IM_FREE(font_data);

            m_font_sampler_id = device.create_sampler({
                .min_filter = graphics::Filtering::Linear,
                .mag_filter = graphics::Filtering::Linear,
                .mip_filter = graphics::Filtering::Linear,
                .address_u = TextureAddressMode::Repeat,
                .address_v = TextureAddressMode::Repeat,
                .min_lod = -1000,
                .max_lod = 1000,
            });
            m_vertex_buffer_id = recreate_buffer(device, BufferUsage::Vertex, 0xfff);
            m_index_buffer_id = recreate_buffer(device, BufferUsage::Index, 0xfff);
        }

        return true;
    }

    void new_frame(f32 width, f32 height, f64 time)
    {
        auto &io = ImGui::GetIO();
        ImGui::NewFrame();

        io.DisplaySize = ImVec2(width, height);
        if (time <= m_last_time)
            time = m_last_time + 0.00001f;
        io.DeltaTime = m_last_time > 0.0 ? (float)(time - m_last_time) : (float)(1.0f / 60.0f);
        m_last_time = time;
    }

    Result<graphics::CommandList, graphics::VKResult> end_frame(graphics::Device &device, graphics::CommandAllocator &cmd_allocator, graphics::ImageViewID target_view_id)
    {
        using namespace lr::graphics;

        ImGui::Render();

        auto &io = ImGui::GetIO();
        CommandQueue &cmd_queue = device.get_queue(CommandType::Graphics);
        ImDrawData *draw_data = ImGui::GetDrawData();

        if (!draw_data) {
            return VKResult::Unknown;
        }

        u64 vertex_size_bytes = draw_data->TotalVtxCount * sizeof(ImDrawVert);
        u64 index_size_bytes = draw_data->TotalIdxCount * sizeof(ImDrawIdx);

        if (vertex_size_bytes == 0) {
            return VKResult::Unknown;
        }

        CommandList cmd_list = {};
        device.create_command_lists({ &cmd_list, 1 }, cmd_allocator);
        device.set_object_name(cmd_list, "ImGui Command List");

        Buffer *vertex_buffer = device.get_buffer(m_vertex_buffer_id);
        Buffer *index_buffer = device.get_buffer(m_index_buffer_id);
        if (vertex_size_bytes > vertex_buffer->m_data_size) {
            device.delete_buffers({ &m_vertex_buffer_id, 1 });
            m_vertex_buffer_id = recreate_buffer(device, BufferUsage::Vertex, vertex_size_bytes + 0xfff);
        }

        if (index_size_bytes > index_buffer->m_data_size) {
            device.delete_buffers({ &m_index_buffer_id, 1 });
            m_index_buffer_id = recreate_buffer(device, BufferUsage::Index, index_size_bytes + 0xfff);
        }

        BufferID temp_vbuffer = device.create_buffer({
            .usage_flags = BufferUsage::Vertex | BufferUsage::TransferSrc,
            .flags = MemoryFlag::HostSeqWrite,
            .data_size = vertex_size_bytes,
        });
        BufferID temp_ibuffer = device.create_buffer({
            .usage_flags = BufferUsage::Index | BufferUsage::TransferSrc,
            .flags = MemoryFlag::HostSeqWrite,
            .data_size = index_size_bytes,
        });

        ImDrawVert *vertex_data = device.buffer_host_data<ImDrawVert>(temp_vbuffer);
        ImDrawIdx *index_data = device.buffer_host_data<ImDrawIdx>(temp_ibuffer);

        for (i32 i = 0; i < draw_data->CmdListsCount; i++) {
            const ImDrawList *im_cmd_list = draw_data->CmdLists[i];
            memcpy(vertex_data, im_cmd_list->VtxBuffer.Data, im_cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
            memcpy(index_data, im_cmd_list->IdxBuffer.Data, im_cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
            vertex_data += im_cmd_list->VtxBuffer.Size;
            index_data += im_cmd_list->IdxBuffer.Size;
        }

        device.begin_command_list(cmd_list);

        cmd_list.memory_barrier({
            .src_access = PipelineAccess::HostWrite,
            .dst_access = PipelineAccess::TransferRead,
        });

        graphics::BufferCopyRegion buffer_region = { .src_offset = 0, .dst_offset = 0 };
        buffer_region.size = vertex_size_bytes;
        cmd_list.copy_buffer_to_buffer(temp_vbuffer, m_vertex_buffer_id, { &buffer_region, 1 });

        buffer_region.size = index_size_bytes;
        cmd_list.copy_buffer_to_buffer(temp_ibuffer, m_index_buffer_id, { &buffer_region, 1 });

        cmd_list.memory_barrier({
            .src_access = PipelineAccess::TransferWrite,
            .dst_access = PipelineAccess::VertexAttrib | PipelineAccess::IndexAttrib,
        });

        cmd_queue.defer({ { temp_vbuffer, temp_ibuffer } });

        RenderingAttachmentInfo attachment = {
            .image_view_id = target_view_id,
            .image_layout = ImageLayout::ColorAttachment,
            .load_op = AttachmentLoadOp::Load,
            .store_op = AttachmentStoreOp::Store,
            .clear_value = {},
        };
        RenderingBeginInfo render_info = {
            .render_area = { 0, 0, u32(io.DisplaySize.x), u32(io.DisplaySize.y) },
            .color_attachments = { &attachment, 1 },
        };
        cmd_list.begin_rendering(render_info);
        cmd_list.set_pipeline(m_pipeline_id);
        cmd_list.set_vertex_buffer(m_vertex_buffer_id);
        cmd_list.set_index_buffer(m_index_buffer_id, 0, true);

        PushConstant pc = {};
        pc.scale = { 2.0f / draw_data->DisplaySize.x, 2.0f / draw_data->DisplaySize.y };
        pc.translate = { -1.0f - draw_data->DisplayPos.x * pc.scale.x, -1.0f - draw_data->DisplayPos.y * pc.scale.y };
        pc.image_view_id = m_font_image_view_id;
        pc.sampler_id = m_font_sampler_id;

        PipelineLayout &pipeline_layout = *device.get_layout<PushConstant>();
        cmd_list.set_push_constants(pipeline_layout, &pc, sizeof(PushConstant), 0);
        cmd_list.set_descriptor_sets(pipeline_layout, graphics::PipelineBindPoint::Graphics, 0, { &device.m_descriptor_set, 1 });
        cmd_list.set_viewport(0, { .x = 0, .y = 0, .width = io.DisplaySize.x, .height = io.DisplaySize.y, .depth_min = 0.01, .depth_max = 1.0 });

        ImVec2 clip_off = draw_data->DisplayPos;
        ImVec2 clip_scale = draw_data->FramebufferScale;
        u32 vertex_offset = 0;
        u32 index_offset = 0;

        for (ImDrawList *draw_list : draw_data->CmdLists) {
            for (i32 cmd_i = 0; cmd_i < draw_list->CmdBuffer.Size; cmd_i++) {
                ImDrawCmd &im_cmd = draw_list->CmdBuffer[cmd_i];

                ImVec2 clip_min((im_cmd.ClipRect.x - clip_off.x) * clip_scale.x, (im_cmd.ClipRect.y - clip_off.y) * clip_scale.y);
                ImVec2 const clip_max((im_cmd.ClipRect.z - clip_off.x) * clip_scale.x, (im_cmd.ClipRect.w - clip_off.y) * clip_scale.y);
                clip_min.x = std::clamp(clip_min.x, 0.0f, static_cast<f32>(io.DisplaySize.x));
                clip_min.y = std::clamp(clip_min.y, 0.0f, static_cast<f32>(io.DisplaySize.y));
                if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y) {
                    continue;
                }

                graphics::Rect2D scissor = {
                    .offset = { i32(clip_min.x), i32(clip_min.y) },
                    .extent = { u32(clip_max.x - clip_min.x), u32(clip_max.y - clip_min.y) },
                };
                cmd_list.set_scissors(0, scissor);
                cmd_list.draw_indexed(im_cmd.ElemCount, im_cmd.IdxOffset + index_offset, i32(im_cmd.VtxOffset + vertex_offset));
            }

            vertex_offset += draw_list->VtxBuffer.Size;
            index_offset += draw_list->IdxBuffer.Size;
        }

        cmd_list.end_rendering();
        device.end_command_list(cmd_list);

        return cmd_list;
    }

    static ImGuiKey lr_key_to_imgui(int key)
    {
        switch (key) {
            case LR_KEY_TAB:
                return ImGuiKey_Tab;
            case LR_KEY_LEFT:
                return ImGuiKey_LeftArrow;
            case LR_KEY_RIGHT:
                return ImGuiKey_RightArrow;
            case LR_KEY_UP:
                return ImGuiKey_UpArrow;
            case LR_KEY_DOWN:
                return ImGuiKey_DownArrow;
            case LR_KEY_PAGE_UP:
                return ImGuiKey_PageUp;
            case LR_KEY_PAGE_DOWN:
                return ImGuiKey_PageDown;
            case LR_KEY_HOME:
                return ImGuiKey_Home;
            case LR_KEY_END:
                return ImGuiKey_End;
            case LR_KEY_INSERT:
                return ImGuiKey_Insert;
            case LR_KEY_DELETE:
                return ImGuiKey_Delete;
            case LR_KEY_BACKSPACE:
                return ImGuiKey_Backspace;
            case LR_KEY_SPACE:
                return ImGuiKey_Space;
            case LR_KEY_ENTER:
                return ImGuiKey_Enter;
            case LR_KEY_ESC:
                return ImGuiKey_Escape;
            case LR_KEY_QUOTE:
                return ImGuiKey_Apostrophe;
            case LR_KEY_COMMA:
                return ImGuiKey_Comma;
            case LR_KEY_MINUS:
                return ImGuiKey_Minus;
            case LR_KEY_DOT:
                return ImGuiKey_Period;
            case LR_KEY_SLASH:
                return ImGuiKey_Slash;
            case LR_KEY_SEMICOLON:
                return ImGuiKey_Semicolon;
            case LR_KEY_EQUAL:
                return ImGuiKey_Equal;
            case LR_KEY_LBRACKET:
                return ImGuiKey_LeftBracket;
            case LR_KEY_BACKSLASH:
                return ImGuiKey_Backslash;
            case LR_KEY_RBRACKET:
                return ImGuiKey_RightBracket;
            case LR_KEY_GRAVE_ACCENT:
                return ImGuiKey_GraveAccent;
            case LR_KEY_CAPS_LOCK:
                return ImGuiKey_CapsLock;
            case LR_KEY_SCROLL_LOCK:
                return ImGuiKey_ScrollLock;
            case LR_KEY_NUM_LOCK:
                return ImGuiKey_NumLock;
            case LR_KEY_PRINT_SCREEN:
                return ImGuiKey_PrintScreen;
            case LR_KEY_PAUSE:
                return ImGuiKey_Pause;
            case LR_KEY_KP_0:
                return ImGuiKey_Keypad0;
            case LR_KEY_KP_1:
                return ImGuiKey_Keypad1;
            case LR_KEY_KP_2:
                return ImGuiKey_Keypad2;
            case LR_KEY_KP_3:
                return ImGuiKey_Keypad3;
            case LR_KEY_KP_4:
                return ImGuiKey_Keypad4;
            case LR_KEY_KP_5:
                return ImGuiKey_Keypad5;
            case LR_KEY_KP_6:
                return ImGuiKey_Keypad6;
            case LR_KEY_KP_7:
                return ImGuiKey_Keypad7;
            case LR_KEY_KP_8:
                return ImGuiKey_Keypad8;
            case LR_KEY_KP_9:
                return ImGuiKey_Keypad9;
            case LR_KEY_KP_DECIMAL:
                return ImGuiKey_KeypadDecimal;
            case LR_KEY_KP_DIVIDE:
                return ImGuiKey_KeypadDivide;
            case LR_KEY_KP_MULTIPLY:
                return ImGuiKey_KeypadMultiply;
            case LR_KEY_KP_SUBTRACT:
                return ImGuiKey_KeypadSubtract;
            case LR_KEY_KP_ADD:
                return ImGuiKey_KeypadAdd;
            case LR_KEY_KP_ENTER:
                return ImGuiKey_KeypadEnter;
            case LR_KEY_KP_EQUAL:
                return ImGuiKey_KeypadEqual;
            case LR_KEY_LSHIFT:
                return ImGuiKey_LeftShift;
            case LR_KEY_LCONTROL:
                return ImGuiKey_LeftCtrl;
            case LR_KEY_LALT:
                return ImGuiKey_LeftAlt;
            case LR_KEY_LSUPER:
                return ImGuiKey_LeftSuper;
            case LR_KEY_RSHIFT:
                return ImGuiKey_RightShift;
            case LR_KEY_RCONTROL:
                return ImGuiKey_RightCtrl;
            case LR_KEY_RALT:
                return ImGuiKey_RightAlt;
            case LR_KEY_RSUPER:
                return ImGuiKey_RightSuper;
            case LR_KEY_MENU:
                return ImGuiKey_Menu;
            case LR_KEY_0:
                return ImGuiKey_0;
            case LR_KEY_1:
                return ImGuiKey_1;
            case LR_KEY_2:
                return ImGuiKey_2;
            case LR_KEY_3:
                return ImGuiKey_3;
            case LR_KEY_4:
                return ImGuiKey_4;
            case LR_KEY_5:
                return ImGuiKey_5;
            case LR_KEY_6:
                return ImGuiKey_6;
            case LR_KEY_7:
                return ImGuiKey_7;
            case LR_KEY_8:
                return ImGuiKey_8;
            case LR_KEY_9:
                return ImGuiKey_9;
            case LR_KEY_A:
                return ImGuiKey_A;
            case LR_KEY_B:
                return ImGuiKey_B;
            case LR_KEY_C:
                return ImGuiKey_C;
            case LR_KEY_D:
                return ImGuiKey_D;
            case LR_KEY_E:
                return ImGuiKey_E;
            case LR_KEY_F:
                return ImGuiKey_F;
            case LR_KEY_G:
                return ImGuiKey_G;
            case LR_KEY_H:
                return ImGuiKey_H;
            case LR_KEY_I:
                return ImGuiKey_I;
            case LR_KEY_J:
                return ImGuiKey_J;
            case LR_KEY_K:
                return ImGuiKey_K;
            case LR_KEY_L:
                return ImGuiKey_L;
            case LR_KEY_M:
                return ImGuiKey_M;
            case LR_KEY_N:
                return ImGuiKey_N;
            case LR_KEY_O:
                return ImGuiKey_O;
            case LR_KEY_P:
                return ImGuiKey_P;
            case LR_KEY_Q:
                return ImGuiKey_Q;
            case LR_KEY_R:
                return ImGuiKey_R;
            case LR_KEY_S:
                return ImGuiKey_S;
            case LR_KEY_T:
                return ImGuiKey_T;
            case LR_KEY_U:
                return ImGuiKey_U;
            case LR_KEY_V:
                return ImGuiKey_V;
            case LR_KEY_W:
                return ImGuiKey_W;
            case LR_KEY_X:
                return ImGuiKey_X;
            case LR_KEY_Y:
                return ImGuiKey_Y;
            case LR_KEY_Z:
                return ImGuiKey_Z;
            case LR_KEY_F1:
                return ImGuiKey_F1;
            case LR_KEY_F2:
                return ImGuiKey_F2;
            case LR_KEY_F3:
                return ImGuiKey_F3;
            case LR_KEY_F4:
                return ImGuiKey_F4;
            case LR_KEY_F5:
                return ImGuiKey_F5;
            case LR_KEY_F6:
                return ImGuiKey_F6;
            case LR_KEY_F7:
                return ImGuiKey_F7;
            case LR_KEY_F8:
                return ImGuiKey_F8;
            case LR_KEY_F9:
                return ImGuiKey_F9;
            case LR_KEY_F10:
                return ImGuiKey_F10;
            case LR_KEY_F11:
                return ImGuiKey_F11;
            case LR_KEY_F12:
                return ImGuiKey_F12;
            case LR_KEY_F13:
                return ImGuiKey_F13;
            case LR_KEY_F14:
                return ImGuiKey_F14;
            case LR_KEY_F15:
                return ImGuiKey_F15;
            case LR_KEY_F16:
                return ImGuiKey_F16;
            case LR_KEY_F17:
                return ImGuiKey_F17;
            case LR_KEY_F18:
                return ImGuiKey_F18;
            case LR_KEY_F19:
                return ImGuiKey_F19;
            case LR_KEY_F20:
                return ImGuiKey_F20;
            case LR_KEY_F21:
                return ImGuiKey_F21;
            case LR_KEY_F22:
                return ImGuiKey_F22;
            case LR_KEY_F23:
                return ImGuiKey_F23;
            case LR_KEY_F24:
                return ImGuiKey_F24;
            default:
                return ImGuiKey_None;
        }
    }
};

}  // namespace lr::example
