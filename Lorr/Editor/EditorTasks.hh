#pragma once

#include "Engine/Core/Application.hh"

#include "Engine/Graphics/Task/TaskGraph.hh"

namespace lr {
struct GridTask {
    std::string_view name = "Grid";

    struct Uses {
        Preset::ColorAttachmentWrite attachment = {};
    } uses = {};

    bool prepare(TaskPrepareInfo &info) {
        auto &app = Application::get();
        auto &pipeline_info = info.pipeline_info;

        auto vs = app.asset_man.shader_at("shader://editor.grid_vs");
        auto fs = app.asset_man.shader_at("shader://editor.grid_fs");
        if (!vs || !fs) {
            LR_LOG_ERROR("Shaders are invalid.", name);
            return false;
        }

        pipeline_info.set_shader(vs.value());
        pipeline_info.set_shader(fs.value());
        pipeline_info.set_dynamic_states(DynamicState::Viewport | DynamicState::Scissor);
        pipeline_info.set_viewport({});
        pipeline_info.set_scissors({});

        return true;
    }

    void execute(TaskContext &tc) {
        auto dst_attachment = tc.as_color_attachment(uses.attachment);

        tc.cmd_list.begin_rendering(RenderingBeginInfo{
            .render_area = tc.pass_rect(),
            .color_attachments = dst_attachment,
        });

        tc.cmd_list.set_viewport(0, tc.pass_viewport());
        tc.cmd_list.set_scissors(0, tc.pass_rect());
        tc.cmd_list.draw(3);
        tc.cmd_list.end_rendering();
    }
};

}  // namespace lr
