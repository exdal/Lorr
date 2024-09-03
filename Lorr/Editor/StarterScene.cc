#include "StarterScene.hh"

#include "Engine/Core/Application.hh"

namespace lr {
bool StarterScene::load(this StarterScene &self) {
    auto camera_entity = self.create_perspective_camera("camera_3d", { 0.0, 2.0, 0.0 }, 65.0f, 1.6f);
    self.set_active_camera(camera_entity);

    auto &app = Application::get();
    auto &world = app.world;
    world.set_sun_direction({ 90.0, 45.0 });

    return true;
}

bool StarterScene::unload(this StarterScene &) {
    return true;
}

void StarterScene::update(this StarterScene &) {
}

}  // namespace lr
