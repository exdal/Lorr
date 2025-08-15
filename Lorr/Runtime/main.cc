#include "Runtime/RuntimeModule.hh"

#include "Engine/Asset/Asset.hh"
#include "Engine/Core/App.hh"
#include "Engine/Graphics/ImGuiRenderer.hh"
#include "Engine/Graphics/VulkanDevice.hh"
#include "Engine/Window/Window.hh"

i32 main(i32 argc, c8 **argv) {
    ZoneScoped;

    if (argc != 2) {
        fmt::println("Invalid world, example: Runtime path/to/world.lrproj");
        return 1;
    }

    auto world_path = fs::path(argv[1]);

    lr::Window::init_sdl();
    auto primary_display = lr::Window::display_at(0).value();

    lr::AppBuilder() //
        .module<lr::Device>(3)
        .module<lr::Window>(lr::WindowInfo{ .title = "Example Game", .width = 1720, .height = 880, .flags = lr::WindowFlag::Centered })
        .module<lr::AssetManager>()
        .module<lr::ImGuiRenderer>()
        .module<lr::SceneRenderer>()
        .module<RuntimeModule>(std::move(world_path))
        .build(8, "runtime.log");

    return 0;
}
