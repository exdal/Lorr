#include "AdvancedApp.hh"

static AdvancedApp app = {};

lr::Application &lr::Application::get()
{
    return app;
}

i32 main(i32 argc, c8 **argv)
{
    ZoneScoped;

    app.init(lr::ApplicationInfo{
        .args = { argv, static_cast<usize>(argc) },
        .window_info = { .title = "Hello GLTF", .width = 1280, .height = 720, .flags = lr::WindowFlag::Centered },
    });
    app.run();

    return 1;
}
