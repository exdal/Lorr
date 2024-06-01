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
        Preset::VertexBuffer vertex_buffer = {};
        Preset::IndexBuffer index_buffer = {};
    } uses = {};

    bool prepare(TaskPrepareInfo &info)
    {
        auto &pipeline_info = info.pipeline_info;

        VirtualFileInfo virtual_files[] = { { "lorr", embedded::shaders::lorr_sp } };
        VirtualDir virtual_dir = { virtual_files };
        ShaderCompileInfo shader_compile_info = {
            .real_path = "imgui.slang",
            .code = embedded::shaders::imgui_str,
            .virtual_env = { { virtual_dir } },
        };

        shader_compile_info.entry_point = "vs_main";
        auto [vs_ir, vs_result] = ShaderCompiler::compile(shader_compile_info);
        shader_compile_info.entry_point = "fs_main";
        auto [fs_ir, fs_result] = ShaderCompiler::compile(shader_compile_info);
        LR_CHECK(!vs_result || !fs_result, "Imgui shaders failed to compile");  // embedded resources fail to compile, its over...
        if (!vs_result || !fs_result) {
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

        return true;
    }

    void execute(TaskContext &tc)
    {
        auto &cmd_list = tc.command_list();

        ImDrawData *draw_data = ImGui::GetDrawData();
        u64 vertex_size_bytes = draw_data->TotalVtxCount * sizeof(ImDrawVert);
        u64 index_size_bytes = draw_data->TotalIdxCount * sizeof(ImDrawIdx);

        // tc.cached_sampler(HSAMPLER({}));
    }
};

}  // namespace lr::graphics::BuiltinTask
