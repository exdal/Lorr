#pragma once

#include "Task.hh"

#include "Engine/Embedded.hh"

#include <imgui.h>

namespace lr::graphics::BuiltinTask {

// TODO: Pipeline cache, sampler cache, transient buffers

struct ImGuiTask {
    std::string_view name = "ImGui";

    struct Uses {
        Preset::ColorAttachmentWrite attachment = {};
        Preset::ColorReadOnly font = {};
    } uses = {};

    struct PushConstants {
        glm::vec2 translate = {};
        glm::vec2 scale = {};
        ImageViewID image_view_id = {};
        SamplerID sampler_id = {};
    } push_constants = {};

    struct {
        SamplerID sampler = SamplerID::Invalid;
        std::array<BufferID, 3> vertex_buffers = {};
        std::array<BufferID, 3> index_buffers = {};
    } self = {};

    bool prepare(TaskPrepareInfo &info)
    {
        auto &pipeline_info = info.pipeline_info;

        VirtualFileInfo virtual_files[] = { { "lorr", embedded::shaders::lorr_sp } };
        VirtualDir virtual_dir = { virtual_files };
        ShaderCompileInfo shader_compile_info = {
            .real_path = "imgui.slang",
            .code = embedded::shaders::imgui_str,
            .virtual_env = { &virtual_dir, 1 },
        };

        shader_compile_info.entry_point = "vs_main";
        auto [vs_ir, vs_result] = ShaderCompiler::compile(shader_compile_info);
        shader_compile_info.entry_point = "fs_main";
        auto [fs_ir, fs_result] = ShaderCompiler::compile(shader_compile_info);
        if (!vs_result || !fs_result) {
            LR_LOG_ERROR("Failed to initialize ImGui pass! {}, {}", vs_result, fs_result);
            return false;
        }

        pipeline_info.set_shader(info.device->create_shader(ShaderStageFlag::Vertex, vs_ir));
        pipeline_info.set_shader(info.device->create_shader(ShaderStageFlag::Fragment, fs_ir));

        pipeline_info.set_dynamic_states(DynamicState::Viewport | DynamicState::Scissor);
        pipeline_info.set_viewport({});
        pipeline_info.set_scissors({});
        pipeline_info.set_vertex_layout({
            { .format = Format::R32G32_SFLOAT, .location = 0, .offset = offsetof(ImDrawVert, pos) },
            { .format = Format::R32G32_SFLOAT, .location = 1, .offset = offsetof(ImDrawVert, uv) },
            { .format = Format::R8G8B8A8_UNORM, .location = 2, .offset = offsetof(ImDrawVert, col) },
        });
        pipeline_info.set_blend_attachment_all({
            .blend_enabled = true,
            .src_blend = BlendFactor::SrcAlpha,
            .dst_blend = BlendFactor::InvSrcAlpha,
            .src_blend_alpha = BlendFactor::One,
            .dst_blend_alpha = BlendFactor::InvSrcAlpha,
        });

        for (BufferID &vertex_buffer : self.vertex_buffers) {
            vertex_buffer = info.device->create_buffer({
                .usage_flags = BufferUsage::Vertex | BufferUsage::TransferDst,
                .flags = MemoryFlag::Dedicated,
                .preference = MemoryPreference::Device,
                .data_size = mib_to_bytes(32),
            });
        }

        for (BufferID &index_buffer : self.index_buffers) {
            index_buffer = info.device->create_buffer({
                .usage_flags = BufferUsage::Index | BufferUsage::TransferDst,
                .flags = MemoryFlag::Dedicated,
                .preference = MemoryPreference::Device,
                .data_size = mib_to_bytes(32),
            });
        }

        self.sampler = info.device->create_cached_sampler({
            .min_filter = graphics::Filtering::Linear,
            .mag_filter = graphics::Filtering::Linear,
            .mip_filter = graphics::Filtering::Linear,
            .address_u = TextureAddressMode::Repeat,
            .address_v = TextureAddressMode::Repeat,
            .min_lod = -1000,
            .max_lod = 1000,
        });

        return true;
    }

    void execute(TaskContext &tc)
    {
        auto device = tc.device();
        auto &task_attachment = tc.task_image_data(uses.attachment);
        auto &font = tc.task_image_data(uses.font);
        auto &cmd_list = tc.command_list();
        auto &staging_buffer = device->frame_staging_buffer();
        auto render_extent = device->get_image(task_attachment.image_id)->m_extent;
        auto &io = ImGui::GetIO();
        ImDrawData *draw_data = ImGui::GetDrawData();

        u64 vertex_size_bytes = draw_data->TotalVtxCount * sizeof(ImDrawVert);
        u64 index_size_bytes = draw_data->TotalIdxCount * sizeof(ImDrawIdx);
        if (!draw_data || vertex_size_bytes == 0) {
            return;
        }

        // TODO: Figure out how to recreate buffer for GPU memory.
        // Using staging buffer(CPU) is not ideal.
        auto vertex_buffer_alloc = staging_buffer.alloc(vertex_size_bytes);
        auto index_buffer_alloc = staging_buffer.alloc(index_size_bytes);
        LR_ASSERT(vertex_buffer_alloc.size >= vertex_size_bytes);
        LR_ASSERT(index_buffer_alloc.size >= index_size_bytes);

        auto vertex_data = reinterpret_cast<ImDrawVert *>(vertex_buffer_alloc.ptr);
        auto index_data = reinterpret_cast<ImDrawIdx *>(index_buffer_alloc.ptr);

        for (i32 i = 0; i < draw_data->CmdListsCount; i++) {
            const ImDrawList *im_cmd_list = draw_data->CmdLists[i];
            memcpy(vertex_data, im_cmd_list->VtxBuffer.Data, im_cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
            memcpy(index_data, im_cmd_list->IdxBuffer.Data, im_cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
            vertex_data += im_cmd_list->VtxBuffer.Size;
            index_data += im_cmd_list->IdxBuffer.Size;
        }

        BufferID vertex_buffer = self.vertex_buffers[device->frame_index()];
        BufferID index_buffer = self.index_buffers[device->frame_index()];
        device->upload_staging(vertex_buffer_alloc, vertex_buffer);
        device->upload_staging(index_buffer_alloc, index_buffer);

        cmd_list.begin_rendering({
            .render_area = { 0, 0, render_extent.width, render_extent.height },
            .color_attachments = { { tc.as_color_attachment(uses.attachment) } },
        });
        cmd_list.set_vertex_buffer(vertex_buffer);
        cmd_list.set_index_buffer(index_buffer, 0, true);

        glm::vec2 scale = { 2.0f / draw_data->DisplaySize.x, 2.0f / draw_data->DisplaySize.y };
        glm::vec2 translate = { -1.0f - draw_data->DisplayPos.x * scale.x, -1.0f - draw_data->DisplayPos.y * scale.y };

        push_constants.scale = scale;
        push_constants.translate = translate;
        push_constants.image_view_id = font.image_view_id;
        push_constants.sampler_id = self.sampler;
        tc.set_push_constants(push_constants);
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
    }
};

}  // namespace lr::graphics::BuiltinTask