#include "EditorApp.hh"

#include "Engine/World/Components.hh"
#include "StarterScene.hh"

namespace lr {
template<typename T>
concept HasIcon = requires { T::ICON; };

template<typename T>
concept NotHasIcon = !requires { T::ICON; };

template<HasIcon T>
constexpr static auto do_get_icon() {
    return T::ICON;
}

template<NotHasIcon T>
constexpr static Icon::detail::icon_t do_get_icon() {
    return nullptr;
}

bool EditorApp::prepare(this EditorApp &self) {
    ZoneScoped;

    self.layout.init();

    auto dummy_scene_id = self.world.create_scene<StarterScene>("starter_scene");
    self.world.set_active_scene(dummy_scene_id);
    self.world.scene_at(dummy_scene_id).do_load();

    auto p = fs::current_path() / ".projects" / "unnamed_world" / "world.lrproj";
    self.world.import_project(p);

    // initialize component icons
    auto get_icon_of = [&self](const auto &v) {
        using T = std::decay_t<decltype(v)>;
        flecs::entity e = self.world.ecs.entity<T>();
        self.component_icons.emplace(e.raw_id(), do_get_icon<T>());
    };

    std::apply(
        [&](const auto &...args) {  //
            (get_icon_of(std::decay_t<decltype(args)>()), ...);
        },
        Component::ALL_COMPONENTS);

    return true;
}

bool EditorApp::update(this EditorApp &self, [[maybe_unused]] f32 delta_time) {
    ZoneScoped;

    self.layout.update();

    return true;
}

}  // namespace lr
