#include "StarterScene.hh"

namespace lr {
auto StarterScene::load() -> bool {
    this->create_perspective_camera("camera_3d", { 0.0, 2.0, 0.0 }, { 0.0, 0.0, 0.0 }, 65.0, 1.6);

    return true;
}

auto StarterScene::unload() -> bool {
    return true;
}

auto StarterScene::update() -> void {
}

}  // namespace lr
