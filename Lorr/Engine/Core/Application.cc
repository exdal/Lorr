#include "Engine/Core/Application.hh"

#include "Engine/OS/Timer.hh"

namespace lr {
bool Application::init(this Application &self, const ApplicationInfo &info) {
    ZoneScoped;

    Logger::init("engine");

    if (!self.do_super_init(info.args)) {
        LOG_FATAL("Super init failed!");
        return false;
    }

    self.job_man.emplace(12);
    self.device.init(3).value();
    self.asset_man = AssetManager::create(&self.device);
    self.window = Window::create(info.window_info);
    auto surface = self.window.get_surface(self.device.get_instance());
    self.swap_chain.emplace(self.device.create_swap_chain(surface).value());
    self.imgui_renderer.init(&self.device);
    self.scene_renderer.init(&self.device);

    if (!self.do_prepare()) {
        LOG_FATAL("Failed to initialize application!");
        return false;
    }

    self.job_man->wait();

    self.run();

    return true;
}

void Application::run(this Application &self) {
    ZoneScoped;

    WindowCallbacks window_callbacks = {};
    window_callbacks.user_data = &self;
    window_callbacks.on_resize = [](void *user_data, glm::uvec2) {
        auto *app = static_cast<Application *>(user_data);
        app->device.wait();

        auto surface = app->window.get_surface(app->device.get_instance());
        app->swap_chain = app->device.create_swap_chain(surface, std::move(app->swap_chain)).value();
    };
    window_callbacks.on_close = [](void *user_data) {
        auto *app = static_cast<Application *>(user_data);
        app->should_close = true;
    };
    window_callbacks.on_mouse_pos = [](void *user_data, glm::vec2 position, glm::vec2) {
        auto *app = static_cast<Application *>(user_data);
        app->imgui_renderer.on_mouse_pos(position);
    };
    window_callbacks.on_mouse_button = [](void *user_data, u8 button, bool down) {
        auto *app = static_cast<Application *>(user_data);
        app->imgui_renderer.on_mouse_button(button, down);
    };
    window_callbacks.on_mouse_scroll = [](void *user_data, glm::vec2 offset) {
        auto *app = static_cast<Application *>(user_data);
        app->imgui_renderer.on_mouse_scroll(offset);
    };
    window_callbacks.on_key = [](void *user_data, SDL_Keycode key_code, SDL_Scancode scan_code, u16 mods, bool down) {
        auto *app = static_cast<Application *>(user_data);
        app->imgui_renderer.on_key(key_code, scan_code, mods, down);
    };
    window_callbacks.on_text_input = [](void *user_data, const c8 *text) {
        auto *app = static_cast<Application *>(user_data);
        app->imgui_renderer.on_text_input(text);
    };

    Timer timer;
    while (!self.should_close) {
        auto delta_time = timer.elapsed();
        timer.reset();

        self.window.poll(window_callbacks);

        auto swapchain_attachment = self.device.new_frame(self.swap_chain.value());
        swapchain_attachment = vuk::clear_image(std::move(swapchain_attachment), vuk::Black<f32>);

        self.imgui_renderer.begin_frame(delta_time, swapchain_attachment->extent);
        self.do_update(delta_time);
        self.do_render(swapchain_attachment->format, swapchain_attachment->extent);

        swapchain_attachment = self.imgui_renderer.end_frame(std::move(swapchain_attachment));
        self.device.end_frame(std::move(swapchain_attachment));

        FrameMark;
    }

    self.shutdown();
}

void Application::shutdown(this Application &self) {
    ZoneScoped;

    LOG_WARN("Shutting down application...");
    self.device.wait();

    self.should_close = true;

    self.do_shutdown();

    self.asset_man.destroy();
    self.scene_renderer.destroy();
    self.swap_chain.reset();
    self.device.destroy();
    LOG_INFO("Complete!");

    Logger::deinit();
}

} // namespace lr
