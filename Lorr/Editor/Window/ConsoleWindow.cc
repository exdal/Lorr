#include "Editor/Window/ConsoleWindow.hh"

// #include "Engine/Core/Application.hh"

namespace led {
// void log_cb(void *user_Data, const loguru::Message &message) {
//     auto *console = static_cast<ConsoleWindow *>(user_Data);
//     auto &m = console->messages.emplace_back();
//     m.verbosity = message.verbosity;
//     m.message = fmt::format("{}", message.message);
// }

ConsoleWindow::ConsoleWindow(std::string name_, bool open_) : IWindow(std::move(name_), open_) {
    // loguru::add_callback("editor", log_cb, this, loguru::Verbosity_MAX);
}

void ConsoleWindow::render(this ConsoleWindow &self) {
    // auto &app = Application::get();
    // auto &render_pipeline = app.world_render_pipeline;

    if (ImGui::Begin(self.name.data())) {
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<i32>(self.messages.size()));
        while (clipper.Step()) {
            for (i32 line_no = clipper.DisplayStart; line_no < clipper.DisplayEnd; line_no++) {
                auto &m = self.messages[line_no];

                // ImGui::PushFont(render_pipeline.im_fa_big);
                // switch (m.verbosity) {
                //     case loguru::Verbosity_WARNING:
                //         ImGui::TextColored(ImVec4(1.0, 1.0, 0.0, 1.0), Icon::fa::triangle_exclamation);
                //         break;
                //     case loguru::Verbosity_ERROR:
                //     case loguru::Verbosity_FATAL:
                //         ImGui::TextColored(ImVec4(0.90234375f, 0.296875f, 0.234375f, 1.0), Icon::fa::circle_exclamation);
                //         break;
                //     default:
                //         ImGui::TextUnformatted(Icon::fa::circle_info);
                //         break;
                // }
                //
                // ImGui::PopFont();

                ImGui::SameLine();
                ImGui::TextUnformatted(m.message.c_str());
                ImGui::Separator();
            }
        }
        clipper.End();

        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
            ImGui::SetScrollHereY(1.0f);
        }
    }

    ImGui::End();
}

} // namespace led
