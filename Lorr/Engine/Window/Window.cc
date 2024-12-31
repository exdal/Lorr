#include "Engine/Window/Window.hh"

#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>
#include <SDL2/SDL_vulkan.h>

namespace lr {
template<>
struct Handle<Window>::Impl {
    u32 width = 0;
    u32 height = 0;

    WindowCursor current_cursor = WindowCursor::Arrow;
    glm::uvec2 cursor_position = {};

    SDL_Window *handle = nullptr;
    u32 monitor_id = WindowInfo::USE_PRIMARY_MONITOR;
    std::array<SDL_Cursor *, usize(WindowCursor::Count)> cursors = {};
};

auto Window::create(const WindowInfo &info) -> Window {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        LOG_ERROR("Failed to initialize SDL! {}", SDL_GetError());
        return Handle(nullptr);
    }

    auto display = Window::display_at(info.monitor);
    if (!display.has_value()) {
        LOG_ERROR("No available displays!");
        return Handle(nullptr);
    }

    i32 new_pos_x = SDL_WINDOWPOS_UNDEFINED;
    i32 new_pos_y = SDL_WINDOWPOS_UNDEFINED;
    i32 new_width = static_cast<i32>(info.width);
    i32 new_height = static_cast<i32>(info.height);

    if (info.flags & WindowFlag::WorkAreaRelative) {
        new_pos_x = display->work_area.x;
        new_pos_y = display->work_area.y;
        new_width = display->work_area.z;
        new_height = display->work_area.w;
    } else if (info.flags & WindowFlag::Centered) {
        new_pos_x = SDL_WINDOWPOS_CENTERED;
        new_pos_y = SDL_WINDOWPOS_CENTERED;
    }

    u32 window_flags = SDL_WINDOW_VULKAN;
    if (info.flags & WindowFlag::Resizable) {
        window_flags |= SDL_WINDOW_RESIZABLE;
    }

    if (info.flags & WindowFlag::Borderless) {
        window_flags |= SDL_WINDOW_BORDERLESS;
    }

    if (info.flags & WindowFlag::Maximized) {
        window_flags |= SDL_WINDOW_MAXIMIZED;
    }

    auto impl = new Impl;
    impl->width = static_cast<u32>(new_width);
    impl->height = static_cast<u32>(new_height);
    impl->monitor_id = info.monitor;
    impl->handle = SDL_CreateWindow(info.title.data(), new_pos_x, new_pos_y, new_width, new_height, window_flags);

    impl->cursors = {
        SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW),    SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_IBEAM),
        SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEALL),  SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENS),
        SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEWE),   SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENESW),
        SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENWSE), SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_HAND),
        SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NO),
    };

    i32 real_width;
    i32 real_height;
    SDL_GetWindowSizeInPixels(impl->handle, &real_width, &real_height);

    impl->width = real_width;
    impl->height = real_height;

    return { impl };
}

auto Window::destroy() -> void {
    ZoneScoped;

    SDL_DestroyWindow(impl->handle);
}

auto Window::poll(const WindowCallbacks &callbacks) -> void {
    ZoneScoped;

    SDL_Event e = {};
    while (SDL_PollEvent(&e) != 0) {
        switch (e.type) {
            case SDL_WINDOWEVENT_RESIZED: {
                if (callbacks.on_resize) {
                    callbacks.on_resize(callbacks.user_data, { e.window.data1, e.window.data2 });
                }
            } break;
            case SDL_MOUSEMOTION: {
                if (callbacks.on_mouse_pos) {
                    callbacks.on_mouse_pos(callbacks.user_data, { e.motion.x, e.motion.y }, { e.motion.xrel, e.motion.yrel });
                }
            } break;
            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP: {
                if (callbacks.on_mouse_button) {
                    Key mouse_key = LR_KEY_MOUSE_1;
                    // clang-format off
                    switch (e.button.button) {
                        case SDL_BUTTON_LEFT: mouse_key = LR_KEY_MOUSE_LEFT; break;
                        case SDL_BUTTON_RIGHT: mouse_key = LR_KEY_MOUSE_RIGHT; break;
                        case SDL_BUTTON_MIDDLE: mouse_key = LR_KEY_MOUSE_MIDDLE; break;
                        case SDL_BUTTON_X1: mouse_key = LR_KEY_MOUSE_4; break;
                        case SDL_BUTTON_X2: mouse_key = LR_KEY_MOUSE_5; break;
                        default: break;
                    }
                    // clang-format on
                    auto state = e.type == SDL_MOUSEBUTTONDOWN ? KeyState::Down : KeyState::Up;
                    callbacks.on_mouse_button(callbacks.user_data, mouse_key, state);
                }
            } break;
            case SDL_MOUSEWHEEL: {
                if (callbacks.on_mouse_scroll) {
                    callbacks.on_mouse_scroll(callbacks.user_data, { e.wheel.preciseX, e.wheel.preciseY });
                }
            } break;
            case SDL_KEYDOWN:
            case SDL_KEYUP: {
                if (callbacks.on_key) {
                    // TODO: We really need a proper input handling
                    callbacks.on_key(callbacks.user_data, {}, {}, {});
                }
            } break;
            case SDL_TEXTINPUT: {
                if (callbacks.on_text_input) {
                    callbacks.on_text_input(callbacks.user_data, e.text.text);
                }
            } break;
            case SDL_QUIT: {
                if (callbacks.on_close) {
                    callbacks.on_close(callbacks.user_data);
                }
            }
            default:
                break;
        }
    }
}

auto Window::set_cursor(WindowCursor cursor) -> void {
    SDL_SetCursor(impl->cursors[usize(cursor)]);
}

auto Window::display_at(i32 monitor_id) -> ls::option<SystemDisplay> {
    ZoneScoped;

    auto display_count = SDL_GetNumVideoDisplays();
    if (monitor_id >= display_count || display_count < 0) {
        return ls::nullopt;
    }

    const char *monitor_name = SDL_GetDisplayName(monitor_id);
    SDL_DisplayMode display_mode = {};
    if (SDL_GetCurrentDisplayMode(monitor_id, &display_mode) != 0) {
        return ls::nullopt;
    }

    SDL_Rect position_bounds = {};
    if (SDL_GetDisplayBounds(monitor_id, &position_bounds) != 0) {
        return ls::nullopt;
    }

    SDL_Rect work_bounds = {};
    if (SDL_GetDisplayUsableBounds(monitor_id, &work_bounds) != 0) {
        return ls::nullopt;
    }

    return SystemDisplay{
        .name = monitor_name,
        .position = { position_bounds.x, position_bounds.y },
        .work_area = { work_bounds.x, work_bounds.y, work_bounds.w, work_bounds.h },
        .resolution = { display_mode.w, display_mode.h },
        .refresh_rate = display_mode.refresh_rate,
    };
}

auto Window::get_size() -> glm::uvec2 {
    return { impl->width, impl->height };
}

auto Window::get_surface(VkInstance instance) -> VkSurfaceKHR {
    ZoneScoped;

    VkSurfaceKHR surface = {};
    SDL_Vulkan_CreateSurface(impl->handle, instance, &surface);
    return surface;
}

}  // namespace lr
