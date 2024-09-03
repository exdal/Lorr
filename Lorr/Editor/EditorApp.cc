#include "EditorApp.hh"

#include "Editor/StarterScene.hh"

namespace lr {
bool EditorApp::prepare(this EditorApp &self) {
    ZoneScoped;

    self.layout.init();

    auto dummy_scene_id = self.world.create_scene<StarterScene>("starter_scene");
    self.world.set_active_scene(dummy_scene_id);
    self.world.scene_at(dummy_scene_id).do_load();

    return true;
}

bool EditorApp::update(this EditorApp &self, [[maybe_unused]] f32 delta_time) {
    ZoneScoped;

    self.layout.update();

    return true;
}

}  // namespace lr
