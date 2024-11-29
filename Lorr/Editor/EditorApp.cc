#include "EditorApp.hh"

#include "StarterScene.hh"

namespace lr {
bool EditorApp::prepare(this EditorApp &self) {
    ZoneScoped;

    self.layout.init();

    auto dummy_scene_id = self.world.add_scene<StarterScene>("starter_scene");
    auto &dummy_scene = self.world.scene(dummy_scene_id.value());
    self.world.set_active_scene(dummy_scene_id.value());
    dummy_scene.load();

    return true;
}

bool EditorApp::update(this EditorApp &self, [[maybe_unused]] f64 delta_time) {
    ZoneScoped;

    self.layout.update();

    return true;
}

}  // namespace lr
