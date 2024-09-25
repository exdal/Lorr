#include "EditorApp.hh"

static lr::EditorApp app = {};

lr::Application &lr::Application::get() {
    return app;
}

i32 main(i32 argc, c8 **argv) {
    ZoneScoped;

    app.init(lr::ApplicationInfo{
        .args = { argv, static_cast<usize>(argc) },
        .window_info = { .title = "Lorr Editor", .width = 1580, .height = 820, .flags = lr::WindowFlag::Centered },
    });
    return 0;
}
