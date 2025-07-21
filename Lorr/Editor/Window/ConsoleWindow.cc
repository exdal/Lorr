#include "Editor/Window/ConsoleWindow.hh"

#include "Engine/Util/Icons/IconsMaterialDesignIcons.hh"

namespace led {

static auto log_messages = std::vector<ls::pair<std::string, fmtlog::LogLevel>>{};

void log_cb(
    [[maybe_unused]] i64 ns,
    [[maybe_unused]] fmtlog::LogLevel level,
    [[maybe_unused]] fmt::string_view location,
    [[maybe_unused]] usize basePos,
    [[maybe_unused]] fmt::string_view threadName,
    [[maybe_unused]] fmt::string_view msg,
    [[maybe_unused]] usize bodyPos,
    [[maybe_unused]] usize logFilePos
) {
    ZoneScoped;

    fmt::println("{}", msg);
    msg.remove_prefix(bodyPos);
    log_messages.emplace_back(std::string(msg.begin(), msg.end()), level);
}

ConsoleWindow::ConsoleWindow(std::string name_, bool open_) : IWindow(std::move(name_), open_) {
    ZoneScoped;

    // acquire log cb
    fmtlog::setLogCB(log_cb, fmtlog::DBG);
}

auto ConsoleWindow::render(this ConsoleWindow &self) -> void {
    ZoneScoped;

    if (ImGui::Begin(self.name.data())) {
        for (const auto &[message, verbosity] : log_messages) {
            auto color = IM_COL32(255, 255, 255, 255);

            switch (verbosity) {
                case fmtlog::DBG: {
                    color = IM_COL32(155, 155, 155, 255);
                    ImGui::Text("  %s |", ICON_MDI_BUG);
                } break;
                case fmtlog::INF: {
                    color = IM_COL32(0, 255, 0, 255);
                    ImGui::Text("  %s |", ICON_MDI_INFORMATION_VARIANT_CIRCLE);
                } break;
                case fmtlog::WRN: {
                    color = IM_COL32(255, 255, 0, 255);
                    ImGui::Text("  %s |", ICON_MDI_ALERT);
                } break;
                case fmtlog::ERR: {
                    color = IM_COL32(255, 0, 0, 255);
                    ImGui::Text("  %s |", ICON_MDI_ALERT_OCTAGON);
                } break;
                default:
                    ImGui::Text("  ");
                    break;
            }

            auto rect_min = ImGui::GetItemRectMin();
            auto spacing = ImGui::GetStyle().FramePadding.x / 2.0f;
            auto *draw_list = ImGui::GetWindowDrawList();
            auto rect_max = ImGui::GetItemRectMax();
            draw_list->AddLine({ rect_min.x + spacing, rect_min.y }, { rect_min.x + spacing, rect_max.y }, color, 4);

            ImGui::SameLine();
            ImGui::TextUnformatted(message.c_str());
        }

        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
            ImGui::SetScrollHereY(1.0f);
        }
    }

    ImGui::End();
}

} // namespace led
