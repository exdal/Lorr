#include "DummyScene.hh"

namespace lr {
bool DummyScene::load(this DummyScene &self) {
    auto camera_entity = self.create_perspective_camera({ 0.0, 2.0, 0.0 }, 65.0f, 1.6f);
    self.set_active_camera(camera_entity);

    return true;
}

bool DummyScene::unload(this DummyScene &) {
    return true;
}

void DummyScene::update(this DummyScene &) {
}

}  // namespace lr
