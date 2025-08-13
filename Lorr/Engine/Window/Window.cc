#include "Engine/Window/Window.hh"

#include "Engine/Core/App.hh"

#include "Engine/Graphics/VulkanDevice.hh"

#include "Engine/Memory/Stack.hh"

#include <SDL3/SDL.h>
#include <SDL3/SDL_dialog.h>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_vulkan.h>

namespace lr {
auto Window::init_sdl() -> bool {
    ZoneScoped;

    return SDL_Init(SDL_INIT_EVENTS | SDL_INIT_VIDEO);
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

Window::Window(const WindowInfo &info) : title(info.title), width(info.width), height(info.height), flags(info.flags), display(info.display) {
    ZoneScoped;
}

auto Window::init(this Window &self) -> bool {
    ZoneScoped;

    i32 new_pos_x = SDL_WINDOWPOS_UNDEFINED;
    i32 new_pos_y = SDL_WINDOWPOS_UNDEFINED;

    if (self.flags & WindowFlag::WorkAreaRelative) {
        LS_EXPECT(self.display != nullptr);

        new_pos_x = self.display->work_area.x;
        new_pos_y = self.display->work_area.y;
        self.width = self.display->work_area.z;
        self.height = self.display->work_area.w;
    } else if (self.flags & WindowFlag::Centered) {
        new_pos_x = SDL_WINDOWPOS_CENTERED;
        new_pos_y = SDL_WINDOWPOS_CENTERED;
    }

    u32 window_flags = SDL_WINDOW_VULKAN;
    if (self.flags & WindowFlag::Resizable) {
        window_flags |= SDL_WINDOW_RESIZABLE;
    }

    if (self.flags & WindowFlag::Borderless) {
        window_flags |= SDL_WINDOW_BORDERLESS;
    }

    if (self.flags & WindowFlag::Maximized) {
        window_flags |= SDL_WINDOW_MAXIMIZED;
    }

    if (self.flags & WindowFlag::Fullscreen) {
        window_flags |= SDL_WINDOW_FULLSCREEN;
    }

    auto window_properties = SDL_CreateProperties();
    LS_DEFER(&) {
        SDL_DestroyProperties(window_properties);
    };

    SDL_SetStringProperty(window_properties, SDL_PROP_WINDOW_CREATE_TITLE_STRING, self.title.c_str());
    SDL_SetNumberProperty(window_properties, SDL_PROP_WINDOW_CREATE_X_NUMBER, new_pos_x);
    SDL_SetNumberProperty(window_properties, SDL_PROP_WINDOW_CREATE_Y_NUMBER, new_pos_y);
    SDL_SetNumberProperty(window_properties, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, self.width);
    SDL_SetNumberProperty(window_properties, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, self.height);
    SDL_SetNumberProperty(window_properties, SDL_PROP_WINDOW_CREATE_FLAGS_NUMBER, window_flags);
    self.handle = SDL_CreateWindowWithProperties(window_properties);

    SDL_GetWindowSizeInPixels(self.handle, &self.width, &self.height);
    SDL_StartTextInput(self.handle);
    self.cursors = {
        SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_DEFAULT),     SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_TEXT),
        SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_MOVE),        SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NS_RESIZE),
        SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_EW_RESIZE),   SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NESW_RESIZE),
        SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NWSE_RESIZE), SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_POINTER),
        SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NOT_ALLOWED),
    };

    self.set_cursor(WindowCursor::Arrow);

    auto &device = App::mod<Device>();
    auto surface = self.get_surface(device.get_instance());
    self.swap_chain.emplace(device.create_swap_chain(surface).value());

    return true;
}

auto Window::destroy(this Window &self) -> void {
    ZoneScoped;

    self.swap_chain.reset();
    SDL_StopTextInput(self.handle);
    SDL_DestroyWindow(self.handle);
}

auto Window::update(this Window &self, f64) -> void {
    ZoneScoped;

    SDL_Event e = {};
    while (SDL_PollEvent(&e) != 0) {
        switch (e.type) {
            case SDL_EVENT_WINDOW_RESIZED: {
                auto &device = App::mod<Device>();

                device.wait();
                auto surface = self.get_surface(device.get_instance());
                self.swap_chain = device.create_swap_chain(surface, std::move(self.swap_chain)).value();
            } break;
            case SDL_EVENT_QUIT: {
                App::close();
            } break;
            default:;
        }

        for (const auto &cb : self.event_listeners) {
            cb(e);
        }
    }
}

auto Window::set_cursor(this Window &self, WindowCursor cursor) -> void {
    ZoneScoped;

    self.current_cursor = cursor;
    SDL_SetCursor(self.cursors[usize(cursor)]);
}

auto Window::get_cursor(this Window &self) -> WindowCursor {
    ZoneScoped;

    return self.current_cursor;
}

auto Window::show_cursor(this Window &, bool show) -> void {
    ZoneScoped;

    show ? SDL_ShowCursor() : SDL_HideCursor();
}

auto Window::get_size(this Window &self) -> glm::ivec2 {
    return { self.width, self.height };
}

auto Window::get_surface(this Window &self, VkInstance instance) -> VkSurfaceKHR {
    ZoneScoped;

    VkSurfaceKHR surface = {};
    if (!SDL_Vulkan_CreateSurface(self.handle, instance, nullptr, &surface)) {
        LOG_ERROR("{}", SDL_GetError());
        return nullptr;
    }

    return surface;
}

auto Window::get_handle(this Window &self) -> void * {
    ZoneScoped;

    auto window_props = SDL_GetWindowProperties(self.handle);

#ifdef LS_LINUX
    const std::string_view video_driver = SDL_GetCurrentVideoDriver();
    if (video_driver == "x11") {
        return reinterpret_cast<void *>(SDL_GetNumberProperty(window_props, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0));
    } else if (video_driver == "wayland") {
        return SDL_GetPointerProperty(window_props, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, nullptr);
    }
#elif LS_WINDOWS
    return SDL_GetPointerProperty(window_props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
#else
    #error platform not supported
#endif

    return nullptr;
}

auto Window::show_dialog(this Window &self, const ShowDialogInfo &info) -> void {
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
    SDL_SetPointerProperty(props, SDL_PROP_FILE_DIALOG_WINDOW_POINTER, self.handle);
    SDL_SetStringProperty(props, SDL_PROP_FILE_DIALOG_LOCATION_STRING, spawn_path_str.c_str());
    SDL_SetBooleanProperty(props, SDL_PROP_FILE_DIALOG_MANY_BOOLEAN, info.multi_select);
    SDL_SetStringProperty(props, SDL_PROP_FILE_DIALOG_TITLE_STRING, stack.null_terminate_cstr(info.title));

    SDL_ShowFileDialogWithProperties(sdl_dialog_kind, info.callback, info.user_data, props);
}

} // namespace lr
