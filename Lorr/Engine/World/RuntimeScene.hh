#pragma once

#include "Engine/World/Scene.hh"

namespace lr {
struct RuntimeScene : Scene {
    RuntimeScene(const std::string &name_, World &world_)
        : Scene(name_, world_) {};

    bool load() override;
    bool unload() override;
    void update() override;
};
}  // namespace lr
