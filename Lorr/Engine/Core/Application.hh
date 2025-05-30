#pragma once

#include "Engine/Asset/Asset.hh"

#include "Engine/Core/JobManager.hh"

#include "Engine/Graphics/ImGuiRenderer.hh"
#include "Engine/Graphics/Vulkan.hh"
#include "Engine/Graphics/VulkanDevice.hh"

#include "Engine/Window/Window.hh"

#include "Engine/Scene/SceneRenderer.hh"

namespace lr {
struct ApplicationInfo {
    ls::span<c8 *> args = {};
    WindowInfo window_info = {};
};

struct Application {
    // Prevent `Application` from being copied, `::get()` only has to return a reference
    Application() = default;
    Application(const Application &) = delete;
    Application(Application &&) = delete;
    Application &operator=(const Application &) = delete;
    Application &operator=(Application &&) = delete;

    static Application &get();

    ls::option<JobManager> job_man = ls::nullopt;
    Device device = {};
    Window window = {};
    ls::option<vuk::Swapchain> swap_chain = ls::nullopt;
    ImGuiRenderer imgui_renderer = {};
    SceneRenderer scene_renderer = {};
    AssetManager asset_man = {};

    bool should_close = false;

    bool init(this Application &, const ApplicationInfo &info);
    void run(this Application &);
    void shutdown(this Application &);

    virtual bool do_super_init(ls::span<c8 *> args) = 0;
    virtual bool do_prepare() = 0;
    virtual bool do_update(f64 delta_time) = 0;
    virtual bool do_render(vuk::Format format, vuk::Extent3D extent) = 0;
    virtual void do_shutdown() = 0;
};

} // namespace lr
