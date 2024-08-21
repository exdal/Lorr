#include "EditorApp.hh"

#include "Editor/DummyScene.hh"

namespace lr {
bool EditorApp::prepare(this EditorApp &self) {
    ZoneScoped;

    self.layout.init();

    auto [dummy_scene_id, dummy_scene] = self.add_scene<DummyScene>("dummy_scene");
    self.set_active_scene(dummy_scene_id);
    dummy_scene->do_load();

    return true;
}

bool EditorApp::update(this EditorApp &self, [[maybe_unused]] f32 delta_time) {
    ZoneScoped;

    self.layout.update();

    return true;
}

}  // namespace lr
