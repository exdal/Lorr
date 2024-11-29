#pragma once

#include "Engine/World/Scene.hh"

namespace lr {
// Dummy scene for Editor's "Load Project" UI.
// Better than displaying full black image to screen.
struct StarterScene : Scene {
    StarterScene(const std::string &name_, World &world_)
        : Scene(name_, world_) {};

    auto load() -> bool override;
    auto unload() -> bool override;
    auto update() -> void override;
};

}  // namespace lr
