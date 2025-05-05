#include "Engine/Window/Window.hh"

#include <SDL3/SDL.h>
#include <SDL3/SDL_dialog.h>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_vulkan.h>

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
    ZoneScoped;

    if (!SDL_Init(SDL_INIT_EVENTS | SDL_INIT_VIDEO)) {
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

    auto window_properties = SDL_CreateProperties();
    LS_DEFER(&) {
        SDL_DestroyProperties(window_properties);
    };

    SDL_SetStringProperty(window_properties, SDL_PROP_WINDOW_CREATE_TITLE_STRING, info.title.c_str());
    SDL_SetNumberProperty(window_properties, SDL_PROP_WINDOW_CREATE_X_NUMBER, new_pos_x);
    SDL_SetNumberProperty(window_properties, SDL_PROP_WINDOW_CREATE_Y_NUMBER, new_pos_y);
    SDL_SetNumberProperty(window_properties, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, new_width);
    SDL_SetNumberProperty(window_properties, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, new_height);
    SDL_SetNumberProperty(window_properties, SDL_PROP_WINDOW_CREATE_FLAGS_NUMBER, window_flags);
    impl->handle = SDL_CreateWindowWithProperties(window_properties);

    impl->cursors = {
        SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_DEFAULT),     SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_TEXT),
        SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_MOVE),        SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NS_RESIZE),
        SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_EW_RESIZE),   SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NESW_RESIZE),
        SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NWSE_RESIZE), SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_POINTER),
        SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NOT_ALLOWED),
    };

    i32 real_width;
    i32 real_height;
    SDL_GetWindowSizeInPixels(impl->handle, &real_width, &real_height);
    SDL_StartTextInput(impl->handle);

    impl->width = real_width;
    impl->height = real_height;

    auto self = Window(impl);
    self.set_cursor(WindowCursor::Arrow);
    return self;
}

auto Window::destroy() -> void {
    ZoneScoped;

    SDL_StopTextInput(impl->handle);
    SDL_DestroyWindow(impl->handle);
}

auto Window::poll(const WindowCallbacks &callbacks) -> void {
    ZoneScoped;

    SDL_Event e = {};
    while (SDL_PollEvent(&e) != 0) {
        switch (e.type) {
            case SDL_EVENT_WINDOW_RESIZED: {
                if (callbacks.on_resize) {
                    callbacks.on_resize(callbacks.user_data, { e.window.data1, e.window.data2 });
                }
            } break;
            case SDL_EVENT_MOUSE_MOTION: {
                if (callbacks.on_mouse_pos) {
                    callbacks.on_mouse_pos(callbacks.user_data, { e.motion.x, e.motion.y }, { e.motion.xrel, e.motion.yrel });
                }
            } break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            case SDL_EVENT_MOUSE_BUTTON_UP: {
                if (callbacks.on_mouse_button) {
                    auto state = e.type == SDL_EVENT_MOUSE_BUTTON_DOWN;
                    callbacks.on_mouse_button(callbacks.user_data, e.button.button, state);
                }
            } break;
            case SDL_EVENT_MOUSE_WHEEL: {
                if (callbacks.on_mouse_scroll) {
                    callbacks.on_mouse_scroll(callbacks.user_data, { e.wheel.x, e.wheel.y });
                }
            } break;
            case SDL_EVENT_KEY_DOWN:
            case SDL_EVENT_KEY_UP: {
                if (callbacks.on_key) {
                    auto state = e.type == SDL_EVENT_KEY_DOWN;
                    callbacks.on_key(callbacks.user_data, e.key.key, e.key.scancode, e.key.mod, state);
                }
            } break;
            case SDL_EVENT_TEXT_INPUT: {
                if (callbacks.on_text_input) {
                    callbacks.on_text_input(callbacks.user_data, e.text.text);
                }
            } break;
            case SDL_EVENT_QUIT: {
                if (callbacks.on_close) {
                    callbacks.on_close(callbacks.user_data);
                }
            } break;
            default:;
        }
    }
}

auto Window::set_cursor(WindowCursor cursor) -> void {
    ZoneScoped;

    impl->current_cursor = cursor;
    SDL_SetCursor(impl->cursors[usize(cursor)]);
}

auto Window::get_cursor() -> WindowCursor {
    ZoneScoped;

    return impl->current_cursor;
}

auto Window::show_cursor(bool show) -> void {
    ZoneScoped;

    show ? SDL_ShowCursor() : SDL_HideCursor();
}

auto Window::display_at(i32 monitor_id) -> ls::option<SystemDisplay> {
    ZoneScoped;

    i32 display_count = 0;
    auto *display_ids = SDL_GetDisplays(&display_count);
    LS_DEFER(&) {
        SDL_free(display_ids);
    };

    if (display_count == 0 || display_ids == nullptr) {
        return ls::nullopt;
    }

    const auto checking_display = display_ids[monitor_id];
    const char *monitor_name = SDL_GetDisplayName(checking_display);
    const auto *display_mode = SDL_GetDesktopDisplayMode(checking_display);
    if (display_mode == nullptr) {
        return ls::nullopt;
    }

    SDL_Rect position_bounds = {};
    if (!SDL_GetDisplayBounds(checking_display, &position_bounds)) {
        return ls::nullopt;
    }

    SDL_Rect work_bounds = {};
    if (!SDL_GetDisplayUsableBounds(checking_display, &work_bounds)) {
        return ls::nullopt;
    }

    return SystemDisplay{
        .name = monitor_name,
        .position = { position_bounds.x, position_bounds.y },
        .work_area = { work_bounds.x, work_bounds.y, work_bounds.w, work_bounds.h },
        .resolution = { display_mode->w, display_mode->h },
        .refresh_rate = display_mode->refresh_rate,
    };
}

auto Window::get_size() -> glm::uvec2 {
    return { impl->width, impl->height };
}

auto Window::get_surface(VkInstance instance) -> VkSurfaceKHR {
    ZoneScoped;

    VkSurfaceKHR surface = {};
    if (!SDL_Vulkan_CreateSurface(impl->handle, instance, nullptr, &surface)) {
        LOG_ERROR("{}", SDL_GetError());
        return nullptr;
    }

    return surface;
}

auto Window::get_handle() -> void * {
    ZoneScoped;

    auto window_props = SDL_GetWindowProperties(impl->handle);

#ifdef LS_LINUX
    const std::string_view video_driver = SDL_GetCurrentVideoDriver();
    if (video_driver == "x11") {
        return reinterpret_cast<void *>(SDL_GetNumberProperty(window_props, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0));
    }
#else
    #error FIXME
#endif

    return nullptr;
}

auto Window::show_dialog(const ShowDialogInfo &info) -> void {
    ZoneScoped;
    memory::ScopedStack stack;

    auto sdl_filters = stack.alloc<SDL_DialogFileFilter>(info.filters.size());
    for (const auto &[filter, sdl_filter] : std::views::zip(info.filters, sdl_filters)) {
        sdl_filter.name = stack.null_terminate_cstr(filter.name);
        sdl_filter.pattern = stack.null_terminate_cstr(filter.pattern);
    }

    auto spawn_path_str = info.spawn_path.string();
    auto sdl_dialog_kind = SDL_FileDialogType{};
    switch (info.kind) {
        case DialogKind::OpenFile:
            sdl_dialog_kind = SDL_FILEDIALOG_OPENFILE;
            break;
        case DialogKind::SaveFile:
            sdl_dialog_kind = SDL_FILEDIALOG_SAVEFILE;
            break;
        case DialogKind::OpenFolder:
            sdl_dialog_kind = SDL_FILEDIALOG_OPENFOLDER;
            break;
    }

    auto props = SDL_CreateProperties();
    LS_DEFER(&) {
        SDL_DestroyProperties(props);
    };

    SDL_SetPointerProperty(props, SDL_PROP_FILE_DIALOG_FILTERS_POINTER, sdl_filters.data());
    SDL_SetNumberProperty(props, SDL_PROP_FILE_DIALOG_NFILTERS_NUMBER, static_cast<i32>(sdl_filters.size()));
    SDL_SetPointerProperty(props, SDL_PROP_FILE_DIALOG_WINDOW_POINTER, impl->handle);
    SDL_SetStringProperty(props, SDL_PROP_FILE_DIALOG_LOCATION_STRING, spawn_path_str.c_str());
    SDL_SetBooleanProperty(props, SDL_PROP_FILE_DIALOG_MANY_BOOLEAN, info.multi_select);
    SDL_SetStringProperty(props, SDL_PROP_FILE_DIALOG_TITLE_STRING, stack.null_terminate_cstr(info.title));

    SDL_ShowFileDialogWithProperties(sdl_dialog_kind, info.callback, info.user_data, props);
}

} // namespace lr
