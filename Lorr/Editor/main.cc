#include "Editor/EditorModule.hh"

#include "Engine/Core/App.hh"
#include "Engine/Graphics/ImGuiRenderer.hh"
#include "Engine/Window/Window.hh"

i32 main(i32, c8 **) {
    ZoneScoped;

    lr::Window::init_sdl();
    auto primary_display = lr::Window::display_at(0).value();

    lr::AppBuilder() //
        .module<lr::Device>(3)
        .module<lr::Window>(
            lr::WindowInfo{ .title = "Lorr Editor", .width = 1720, .height = 880, .flags = lr::WindowFlag::Centered | lr::WindowFlag::Resizable }
        )
        .module<lr::AssetManager>()
        .module<lr::ImGuiRenderer>()
        .module<lr::SceneRenderer>()
        .module<led::EditorModule>()
        .build(8, "editor.log");

    return 0;
}
