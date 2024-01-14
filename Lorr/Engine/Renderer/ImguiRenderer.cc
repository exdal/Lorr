#include "ImguiRenderer.hh"

#include "Utils/Timer.hh"

namespace lr::Graphics
{
void ImguiRenderer::init()
{
    ZoneScoped;

    ImGui::CreateContext();

    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = nullptr;

    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;
    io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;

    io.BackendPlatformName = "imgui_impl_lorr";

    ImGui::StyleColorsDark();
}

void ImguiRenderer::destroy()
{
    ZoneScoped;
}

void ImguiRenderer::new_frame(f32 width, f32 height)
{
    ZoneScoped;

    static Timer<f32> timer = {};
    ImGui::GetMainViewport();
    ImGuiIO &io = ImGui::GetIO();

    io.DisplaySize = ImVec2(width, height);
    io.DeltaTime = timer.elapsed();
    timer.reset();

    ImGui::NewFrame();
}

void ImguiRenderer::end_frame()
{
    ZoneScoped;

    ImGui::EndFrame();
    ImGui::Render();
}
}  // namespace lr::Graphics