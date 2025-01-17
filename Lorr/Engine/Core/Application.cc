#include "Application.hh"

#include "Engine/OS/Timer.hh"

namespace lr {
bool Application::init(this Application &self, const ApplicationInfo &info) {
    ZoneScoped;

    Logger::init("engine");

    if (!self.do_super_init(info.args)) {
        LOG_FATAL("Super init failed!");
        return false;
    }

    self.device.init(3).value();
    self.asset_man = AssetManager::create(&self.device);

    self.window = Window::create(info.window_info);
    auto window_size = self.window.get_size();
    auto surface = self.window.get_surface(self.device.get_instance());
    self.swapchain = SwapChain::create(self.device, surface, { .width = window_size.x, .height = window_size.y }).value();
    self.imgui_renderer.init(&self.device);
    self.scene_renderer.init(&self.device);

    if (!self.do_prepare()) {
        LOG_FATAL("Failed to initialize application!");
        return false;
    }

    self.run();
    return true;
}

void Application::run(this Application &self) {
    ZoneScoped;

    WindowCallbacks window_callbacks = {};
    window_callbacks.user_data = &self;
    window_callbacks.on_close = [](void *user_data) {
        auto app = static_cast<Application *>(user_data);
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
    window_callbacks.on_text_input = [](void *user_data, c8 *text) {
        auto *app = static_cast<Application *>(user_data);
        app->imgui_renderer.on_text_input(text);
    };

    Timer timer;
    b32 swapchain_ok = true;
    while (!self.should_close) {
        auto delta_time = timer.elapsed();
        timer.reset();

        if (!swapchain_ok) {
            auto window_size = self.window.get_size();
            auto surface = self.window.get_surface(self.device.get_instance());
            self.swapchain = SwapChain::create(self.device, surface, { .width = window_size.x, .height = window_size.y }).value();

            swapchain_ok = true;
        }

        auto swapchain_attachment = self.device.new_frame(self.swapchain);
        swapchain_attachment = vuk::clear_image(std::move(swapchain_attachment), vuk::Black<f32>);

        // Delta time
        self.window.poll(window_callbacks);

        self.imgui_renderer.begin_frame(delta_time, self.swapchain.extent());
        self.do_update(delta_time);
        if (self.active_scene_uuid.has_value()) {
            auto *scene = self.asset_man.get_scene(self.active_scene_uuid.value());
            scene->tick();

            scene->upload_scene(self.scene_renderer);
        }
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

    self.should_close = true;

    self.do_shutdown();

    self.active_scene_uuid.reset();
    self.device.wait();
    self.swapchain.destroy();
    self.asset_man.destroy();
    self.device.destroy();
    LOG_INFO("Complete!");

    Logger::deinit();
}

}  // namespace lr
