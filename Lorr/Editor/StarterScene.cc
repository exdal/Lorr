#include "StarterScene.hh"

namespace lr {
bool StarterScene::load(this StarterScene &self) {
    auto camera_entity = self.create_perspective_camera("camera_3d", { 0.0, 2.0, 0.0 }, 65.0f, 1.6f);
    self.set_active_camera(camera_entity);
    self.create_directional_light("directional_light", { 45.0, 45.0 });

    return true;
}

bool StarterScene::unload(this StarterScene &) {
    return true;
}

void StarterScene::update(this StarterScene &) {
}

}  // namespace lr
