#include "GameApp.hh"

void GameApp::Init(lr::ApplicationDesc &desc)
{
    ZoneScoped;

    m_Engine.Init(desc.Window);
}

void GameApp::Shutdown()
{
    ZoneScoped;
}

void GameApp::Poll(f32 deltaTime)
{
    ZoneScoped;

    ImGui::Begin("Dear ImGui Demo", nullptr);

    ImGui::PushItemWidth(ImGui::GetFontSize() * -12);

    // Menu Bar

    ImGui::Text("dear imgui says hello! (%s) (%d)", IMGUI_VERSION, IMGUI_VERSION_NUM);
    ImGui::Spacing();

    ImGui::Text("ABOUT THIS DEMO:");
    ImGui::BulletText("Sections below are demonstrating many aspects of the library.");
    ImGui::BulletText("The \"Examples\" menu above leads to more demo contents.");
    ImGui::BulletText("The \"Tools\" menu above gives access to: About Box, Style Editor,\n"
                      "and Metrics/Debugger (general purpose Dear ImGui debugging tool).");
    ImGui::Separator();

    ImGui::Text("PROGRAMMER GUIDE:");
    ImGui::BulletText("See the ShowDemoWindow() code in imgui_demo.cpp. <- you are here!");
    ImGui::BulletText("See comments in imgui.cpp.");
    ImGui::BulletText("See example applications in the examples/ folder.");
    ImGui::BulletText("Read the FAQ at http://www.dearimgui.org/faq/");
    ImGui::BulletText("Set 'io.ConfigFlags |= NavEnableKeyboard' for keyboard controls.");
    ImGui::BulletText("Set 'io.ConfigFlags |= NavEnableGamepad' for gamepad controls.");

    ImGui::End();
}
